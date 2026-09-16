#include "framegen/ml_blend.h"

#include <algorithm>
#include <vector>

namespace framegen {

namespace {

void barrier() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

std::string convDefines(const ml::ConvLayer& layer) {
    return "#define IN_LAYERS " + std::to_string(ml::packedLayers(layer.cin)) +
           "\n#define OUT_LAYERS " + std::to_string(ml::packedLayers(layer.cout)) +
           "\n#define TAPS " + std::to_string(layer.taps()) + "\n#define RELU 1";
}

std::string resampleDefines(int channels, int mode) {
    return "#define LAYERS " + std::to_string(ml::packedLayers(channels)) + "\n#define MODE " +
           std::to_string(mode);
}

// std430 image of one layer: mat4 w[OUT * TAPS * IN]; vec4 b[OUT]. A mat4 is
// column-major, so `m * x` sums m[column][row] * x[column]: the input channel
// is the column, the output channel the row. With `mean`/`scale` given, the
// input normalisation is folded in, which leaves the shader nothing to do but
// the convolution.
std::vector<float> packLayer(const ml::ConvLayer& layer, const float* mean, const float* scale) {
    const int inLayers = ml::packedLayers(layer.cin);
    const int outLayers = ml::packedLayers(layer.cout);
    const int taps = layer.taps();
    const size_t biasBase = static_cast<size_t>(outLayers) * taps * inLayers * 16;
    std::vector<float> data(biasBase + static_cast<size_t>(outLayers) * 4, 0.f);
    for (int o = 0; o < layer.cout; ++o) {
        double bias = layer.b[o];
        for (int i = 0; i < layer.cin; ++i) {
            const float s = scale ? scale[i] : 1.f;
            for (int t = 0; t < taps; ++t) {
                const float w = layer.w[layer.index(o, i, t)];
                const size_t matrix = (static_cast<size_t>(o / 4) * taps + t) * inLayers + i / 4;
                data[matrix * 16 + (i % 4) * 4 + o % 4] = w * s;
                if (mean) bias -= static_cast<double>(w) * s * mean[i];
            }
        }
        data[biasBase + static_cast<size_t>(o / 4) * 4 + o % 4] = static_cast<float>(bias);
    }
    return data;
}

}  // namespace

bool MlBlend::create(const std::string& weights, bool dump) {
    dump_ = dump;
    const bool network = !weights.empty();
    if (network && !net_.load(weights)) {
        std::fprintf(stderr, "[ml] cannot load weights from %s\n", weights.c_str());
        return false;
    }

    std::string featureDefines;
    if (dump) featureDefines += "#define WRITE_FEATURES 1\n#define DUMP_CANDIDATES 1\n";
    if (network)
        featureDefines += "#define ENC0_LAYERS " + std::to_string(ml::packedLayers(net_.c0)) + "\n";
    features_ = gfx::Program::compute("fg_ml_features.comp", featureDefines);
    bool ok = features_.valid();
    if (!network) return ok;

    for (int l = ml::kEnc1; l < ml::kDec0; ++l) {
        conv_[l] = gfx::Program::compute("ml_conv.comp", convDefines(net_.layers[l]));
        ok = ok && conv_[l].valid();
    }
    pool0_ = gfx::Program::compute("ml_resample.comp", resampleDefines(net_.c0, 1));
    pool1_ = gfx::Program::compute("ml_resample.comp", resampleDefines(net_.c1, 1));
    upSum1_ = gfx::Program::compute("ml_resample.comp", resampleDefines(net_.c1, 2));
    blend_ = gfx::Program::compute(
        "fg_ml_blend.comp", "#define HIDDEN_LAYERS " + std::to_string(ml::packedLayers(net_.c0)));
    ok = ok && pool0_.valid() && pool1_.valid() && upSum1_.valid() && blend_.valid();

    glCreateBuffers(ml::kLayerCount, buffers_);
    for (int l = 0; l < ml::kLayerCount; ++l) {
        const bool first = l == ml::kEnc0;
        const std::vector<float> data = packLayer(net_.layers[l], first ? net_.mean.data() : nullptr,
                                                  first ? net_.scale.data() : nullptr);
        glNamedBufferStorage(buffers_[l], static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                             data.data(), 0);
    }
    loaded_ = ok;
    std::printf("[ml] %s: c0 %d, c1 %d, %zu parameters%s\n", weights.c_str(), net_.c0, net_.c1,
                net_.parameters(), ok ? "" : " (shader build failed)");
    return ok;
}

void MlBlend::destroy() {
    features_.destroy();
    for (gfx::Program& p : conv_) p.destroy();
    pool0_.destroy();
    pool1_.destroy();
    upSum1_.destroy();
    blend_.destroy();
    if (buffers_[0]) glDeleteBuffers(ml::kLayerCount, buffers_);
    std::fill(std::begin(buffers_), std::end(buffers_), 0u);
    for (gfx::Texture2DArray* t : {&featureMaps_, &candidates_, &enc0_, &pooled0_, &enc1_, &pooled1_,
                                   &enc2_, &enc3_, &sum1_, &dec1_})
        t->destroy();
    output_.destroy();
    weights_.destroy();
    weightView_ = false;
    loaded_ = dump_ = fits_ = lastValid_ = headerWritten_ = false;
    width_ = height_ = 0;
}

void MlBlend::reloadShaders() {
    features_.reloadIfChanged();
    if (!loaded_) return;
    for (int l = ml::kEnc1; l < ml::kDec0; ++l) conv_[l].reloadIfChanged();
    pool0_.reloadIfChanged();
    pool1_.reloadIfChanged();
    upSum1_.reloadIfChanged();
    blend_.reloadIfChanged();
}

void MlBlend::ensure(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    if (dump_) {
        featureMaps_.ensure(width, height, ml::packedLayers(ml::kFeatures), GL_RGBA16F, "ml.features");
        candidates_.ensure(width, height, 6, GL_RGBA16F, "ml.candidates");
    }
    if (!loaded_) return;
    const int l0 = ml::packedLayers(net_.c0);
    const int l1 = ml::packedLayers(net_.c1);
    // Written by the feature pass whenever the network is loaded.
    enc0_.ensure(width, height, l0, GL_RGBA16F, "ml.enc0");
    // Two poolings: a size not divisible by 4 would pool on a grid the
    // trainer never saw.
    fits_ = width % 4 == 0 && height % 4 == 0;
    if (!fits_) {
        std::fprintf(stderr, "[ml] %dx%d is not divisible by 4; using the heuristic\n", width, height);
        return;
    }
    pooled0_.ensure(width / 2, height / 2, l0, GL_RGBA16F, "ml.pooled0");
    enc1_.ensure(width / 2, height / 2, l1, GL_RGBA16F, "ml.enc1");
    pooled1_.ensure(width / 4, height / 4, l1, GL_RGBA16F, "ml.pooled1");
    enc2_.ensure(width / 4, height / 4, l1, GL_RGBA16F, "ml.enc2");
    enc3_.ensure(width / 4, height / 4, l1, GL_RGBA16F, "ml.enc3");
    sum1_.ensure(width / 2, height / 2, l1, GL_RGBA16F, "ml.sum1");
    dec1_.ensure(width / 2, height / 2, l0, GL_RGBA16F, "ml.dec1");
    output_.ensure(width, height, GL_RGBA16F, 1, "ml.output");
    // Allocated with everything else, whether or not the view is ever shown:
    // see fg_ml_blend.comp for what allocating it mid-run did.
    weights_.ensure(width, height, GL_RGBA8, 1, "ml.weights");
}

void MlBlend::bindCommon(const gfx::Program& program, const MlInputs& in) const {
    in.current->bindTexture(0);
    in.previous->bindTexture(1);
    in.gameField->bindTexture(2);
    in.flowField->bindTexture(3);
    in.masks->bindTexture(4);
    in.heuristic->bindTexture(5);
    program.set("uDisplaySize", width_, height_);
    program.set("uFieldSize", in.fieldWidth, in.fieldHeight);
    program.set("uLevels", in.levels);
    program.set("uGameEnabled", in.game ? 1 : 0);
    program.set("uFlowEnabled", in.flow ? 1 : 0);
    program.set("uMaskEnabled", in.masksEnabled ? 1 : 0);
}

bool MlBlend::dispatch(const MlInputs& in, gfx::GpuTimer& timer) {
    ensure(in.heuristic->width, in.heuristic->height);
    lastValid_ = false;
    {
        gfx::GpuScope scope(timer, loaded_ ? "FG ML features + enc0" : "FG ML features");
        features_.bind();
        bindCommon(features_, in);
        if (dump_) {
            featureMaps_.bindImage(0, GL_WRITE_ONLY);
            candidates_.bindImage(1, GL_WRITE_ONLY);
        }
        if (loaded_) {
            enc0_.bindImage(2, GL_WRITE_ONLY);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers_[ml::kEnc0]);
        }
        features_.dispatch(width_, height_);
        barrier();
    }
    if (!loaded_ || !fits_) return false;

    gfx::GpuScope scope(timer, "FG ML network");
    auto resample = [&](const gfx::Program& program, const gfx::Texture2DArray& input,
                        const gfx::Texture2DArray* skip, const gfx::Texture2DArray& output,
                        const char* name) {
        gfx::GpuScope pass(timer, name);
        program.bind();
        input.bindTexture(0);
        program.set("uInput", 0);
        if (skip) {
            skip->bindTexture(1);
            program.set("uSkip", 1);
        }
        output.bindImage(0, GL_WRITE_ONLY);
        program.set("uOutSize", output.width, output.height);
        program.dispatch(output.width, output.height);
        barrier();
    };
    auto convolve = [&](int layer, const gfx::Texture2DArray& input, const gfx::Texture2DArray& output,
                        const char* name) {
        gfx::GpuScope pass(timer, name);
        conv_[layer].bind();
        input.bindTexture(0);
        conv_[layer].set("uInput", 0);
        output.bindImage(0, GL_WRITE_ONLY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers_[layer]);
        conv_[layer].set("uOutSize", output.width, output.height);
        conv_[layer].dispatch(output.width, output.height);
        barrier();
    };
    resample(pool0_, enc0_, nullptr, pooled0_, "ML pool0");
    convolve(ml::kEnc1, pooled0_, enc1_, "ML enc1");
    resample(pool1_, enc1_, nullptr, pooled1_, "ML pool1");
    convolve(ml::kEnc2, pooled1_, enc2_, "ML enc2");
    convolve(ml::kEnc3, enc2_, enc3_, "ML enc3");
    resample(upSum1_, enc3_, &enc1_, sum1_, "ML up1 + skip");
    convolve(ml::kDec1, sum1_, dec1_, "ML dec1");

    {
        gfx::GpuScope pass(timer, "ML dec0 + blend");
        blend_.bind();
        bindCommon(blend_, in);
        enc0_.bindTexture(6);
        dec1_.bindTexture(7);
        output_.bindImage(0, GL_WRITE_ONLY);
        weights_.bindImage(1, GL_WRITE_ONLY);
        blend_.set("uWriteWeights", weightView_ ? 1 : 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers_[ml::kDec0]);
        blend_.dispatch(width_, height_);
        barrier();
    }
    lastValid_ = true;
    return true;
}

bool MlBlend::writeDatasetHeader(std::FILE* file, int patch, bool withMlOutput, int width,
                                 int height) {
    ml::DatasetHeader header;
    header.patch = patch;
    header.extra = withMlOutput ? 3 : 0;
    header.frameWidth = width;
    header.frameHeight = height;
    return std::fwrite(&header, sizeof(header), 1, file) == 1;
}

void MlBlend::dumpPatches(std::FILE* file, long long frame, const gfx::Texture2D& heuristic,
                          const gfx::Texture2D& reference, int patch, int count, std::mt19937& rng,
                          bool withMlOutput) {
    const int w = heuristic.width;
    const int h = heuristic.height;
    if (!dump_ || !candidates_.valid() || patch > w || patch > h || count <= 0) return;
    // The file header promises the engine's output in every record.
    if (withMlOutput && !lastValid_) return;
    if (!headerWritten_) {
        writeDatasetHeader(file, patch, withMlOutput, w, h);
        headerWritten_ = true;
    }

    std::vector<float> heur(static_cast<size_t>(w) * h * 4);
    std::vector<float> ref(heur.size());
    glGetTextureImage(heuristic.id, 0, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(heur.size() * sizeof(float)),
                      heur.data());
    glGetTextureImage(reference.id, 0, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(ref.size() * sizeof(float)),
                      ref.data());

    // A grid of non-overlapping cells at a random offset, so patch borders do
    // not always fall on the same pixels. The offset is a multiple of 4: the
    // network pools twice, and a patch has to pool on the same grid the frame
    // does for the trainer's output to be comparable with the shader's.
    const int ox = 4 * std::uniform_int_distribution<int>(0, (w % patch) / 4)(rng);
    const int oy = 4 * std::uniform_int_distribution<int>(0, (h % patch) / 4)(rng);
    const int nx = (w - ox) / patch;
    const int ny = (h - oy) / patch;
    std::vector<double> error(static_cast<size_t>(nx) * ny, 0.0);
    for (int cy = 0; cy < ny; ++cy)
        for (int cx = 0; cx < nx; ++cx) {
            double sum = 0.0;
            for (int y = oy + cy * patch; y < oy + (cy + 1) * patch; ++y) {
                const float* a = heur.data() + (static_cast<size_t>(y) * w + ox + cx * patch) * 4;
                const float* b = ref.data() + (static_cast<size_t>(y) * w + ox + cx * patch) * 4;
                for (int x = 0; x < patch; ++x, a += 4, b += 4)
                    sum += std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
            }
            error[static_cast<size_t>(cy) * nx + cx] = sum;
        }

    // Half the patches uniformly, half where the heuristic is wrong: uniform
    // alone is mostly static wall on which every candidate is equally right,
    // error-drawn alone would teach a network that never sees an easy pixel.
    std::vector<std::pair<int, int>> chosen;  // cell, selection
    std::vector<bool> taken(error.size(), false);
    const int uniformCount = std::min(count / 2 + count % 2, static_cast<int>(error.size()));
    std::uniform_int_distribution<int> anyCell(0, static_cast<int>(error.size()) - 1);
    for (int attempts = 0; static_cast<int>(chosen.size()) < uniformCount && attempts < 1000; ++attempts) {
        const int c = anyCell(rng);
        if (taken[c]) continue;
        taken[c] = true;
        chosen.push_back({c, 0});
    }
    while (static_cast<int>(chosen.size()) < count) {
        std::vector<double> weights(error.size());
        double total = 0.0;
        for (size_t i = 0; i < error.size(); ++i) {
            weights[i] = taken[i] ? 0.0 : error[i];
            total += weights[i];
        }
        if (total <= 0.0) break;
        const int c = std::discrete_distribution<int>(weights.begin(), weights.end())(rng);
        taken[c] = true;
        chosen.push_back({c, 1});
    }

    const size_t pixels = static_cast<size_t>(patch) * patch;
    std::vector<float> features(pixels * ml::packedLayers(ml::kFeatures) * 4);
    std::vector<float> candidates(pixels * 6 * 4);
    std::vector<float> mlOut(pixels * 4);
    const bool withMl = withMlOutput;
    std::vector<uint16_t> record;
    for (const auto& [cell, selection] : chosen) {
        const int x = ox + (cell % nx) * patch;
        const int y = oy + (cell / nx) * patch;
        glGetTextureSubImage(featureMaps_.id, 0, x, y, 0, patch, patch, ml::packedLayers(ml::kFeatures),
                             GL_RGBA, GL_FLOAT, static_cast<GLsizei>(features.size() * sizeof(float)),
                             features.data());
        glGetTextureSubImage(candidates_.id, 0, x, y, 0, patch, patch, 6, GL_RGBA, GL_FLOAT,
                             static_cast<GLsizei>(candidates.size() * sizeof(float)), candidates.data());
        if (withMl)
            glGetTextureSubImage(output_.id, 0, x, y, 0, patch, patch, 1, GL_RGBA, GL_FLOAT,
                                 static_cast<GLsizei>(mlOut.size() * sizeof(float)), mlOut.data());

        ml::PatchRecord rec;
        rec.frame = static_cast<int32_t>(frame);
        rec.x = x;
        rec.y = y;
        rec.selection = selection;
        std::fwrite(&rec, sizeof(rec), 1, file);

        const int channels = ml::kFeatures + 3 * ml::kCandidates + 3 + (withMl ? 3 : 0);
        record.resize(pixels * channels);
        size_t n = 0;
        for (size_t p = 0; p < pixels; ++p) {
            const size_t row = p / patch;
            const size_t col = p % patch;
            for (int c = 0; c < ml::kFeatures; ++c)
                record[n++] = ml::toHalf(features[((c / 4) * pixels + p) * 4 + c % 4]);
            for (int c = 0; c < 3 * ml::kCandidates; ++c)
                record[n++] = ml::toHalf(candidates[((c / 4) * pixels + p) * 4 + c % 4]);
            const size_t g = ((y + row) * w + x + col) * 4;
            for (int c = 0; c < 3; ++c) record[n++] = ml::toHalf(ref[g + c]);
            if (withMl)
                for (int c = 0; c < 3; ++c) record[n++] = ml::toHalf(mlOut[p * 4 + c]);
        }
        std::fwrite(record.data(), sizeof(uint16_t), record.size(), file);
    }
}

}  // namespace framegen
