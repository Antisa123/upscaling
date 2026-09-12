#include "metrics/image_metrics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace metrics {

void ImageMetrics::create() {
    reduce_ = gfx::Program::compute("metrics_reduce.comp");
    ssimPrepare_ = gfx::Program::compute("metrics_ssim_prepare.comp");
    gaussian_ = gfx::Program::compute("metrics_gaussian.comp");
    ssimReduce_ = gfx::Program::compute("metrics_ssim_reduce.comp");
}

void ImageMetrics::reloadShaders() {
    reduce_.reloadIfChanged();
    ssimPrepare_.reloadIfChanged();
    gaussian_.reloadIfChanged();
    ssimReduce_.reloadIfChanged();
}

void ImageMetrics::destroy() {
    reduce_.destroy();
    ssimPrepare_.destroy();
    gaussian_.destroy();
    ssimReduce_.destroy();
    for (int i = 0; i < 2; ++i) {
        stats0_[i].destroy();
        stats1_[i].destroy();
    }
    if (partials_) glDeleteBuffers(1, &partials_);
    partials_ = 0;
    partialCapacity_ = 0;
}

void ImageMetrics::ensurePartials(int groupCount) {
    if (groupCount <= partialCapacity_) return;
    if (partials_) glDeleteBuffers(1, &partials_);
    glCreateBuffers(1, &partials_);
    glNamedBufferStorage(partials_, static_cast<GLsizeiptr>(groupCount) * 4 * sizeof(float),
                         nullptr, GL_MAP_READ_BIT);
    partialCapacity_ = groupCount;
    gfx::objectLabel(GL_BUFFER, partials_, "metrics.partials");
}

CompareResult ImageMetrics::compare(const gfx::Texture2D& a, const gfx::Texture2D& b, Mode mode,
                                    const gfx::Texture2D* velocity, bool tonemap) {
    CompareResult result;
    if (!reduce_.valid() || !a.valid() || !b.valid()) return result;
    if (a.width != b.width || a.height != b.height) {
        std::fprintf(stderr, "[metrics] size mismatch: %dx%d vs %dx%d\n", a.width, a.height,
                     b.width, b.height);
        return result;
    }

    const int groupsX = (a.width + kTile - 1) / kTile;
    const int groupsY = (a.height + kTile - 1) / kTile;
    const int groupCount = groupsX * groupsY;

    // Each group writes vec4(sumSq, sumAbs, maxErr, validPixels).
    ensurePartials(groupCount);

    reduce_.bind();
    a.bindTexture(0);
    b.bindTexture(1);
    if (velocity && velocity->valid()) velocity->bindTexture(2);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, partials_);
    reduce_.set("uMode", mode == Mode::Reprojected ? 1 : 0);
    reduce_.set("uTonemap", tonemap ? 1 : 0);
    reduce_.set("uSize", static_cast<float>(a.width), static_cast<float>(a.height));
    glDispatchCompute(static_cast<GLuint>(groupsX), static_cast<GLuint>(groupsY), 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    std::vector<float> partials(static_cast<size_t>(groupCount) * 4);
    glGetNamedBufferSubData(partials_, 0, static_cast<GLsizeiptr>(partials.size() * sizeof(float)),
                            partials.data());

    double sumSq = 0.0, sumAbs = 0.0, maxErr = 0.0, valid = 0.0;
    for (int i = 0; i < groupCount; ++i) {
        sumSq += partials[i * 4 + 0];
        sumAbs += partials[i * 4 + 1];
        maxErr = std::max(maxErr, static_cast<double>(partials[i * 4 + 2]));
        valid += partials[i * 4 + 3];
    }

    const double samples = std::max(1.0, valid * 3.0);  // three colour channels
    result.mse = sumSq / samples;
    result.mae = sumAbs / samples;
    result.maxError = maxErr;
    result.coverage = valid / (static_cast<double>(a.width) * a.height);
    result.psnr = result.mse > 0.0 ? 10.0 * std::log10(1.0 / result.mse) : 99.0;

    if (computeSsim_) computeSsim(a, b, mode, velocity, tonemap, result);
    return result;
}

void ImageMetrics::computeSsim(const gfx::Texture2D& a, const gfx::Texture2D& b, Mode mode,
                               const gfx::Texture2D* velocity, bool tonemap,
                               CompareResult& result) {
    if (!ssimPrepare_.valid() || !gaussian_.valid() || !ssimReduce_.valid()) return;

    const int w = a.width;
    const int h = a.height;
    for (int i = 0; i < 2; ++i) {
        stats0_[i].ensure(w, h, GL_RGBA32F, 1, "metrics.ssim.stats0." + std::to_string(i));
        stats1_[i].ensure(w, h, GL_RG32F, 1, "metrics.ssim.stats1." + std::to_string(i));
    }

    // Stage 1: per-pixel moments.
    ssimPrepare_.bind();
    a.bindTexture(0);
    b.bindTexture(1);
    if (velocity && velocity->valid()) velocity->bindTexture(2);
    stats0_[0].bindImage(0, GL_WRITE_ONLY);
    stats1_[0].bindImage(1, GL_WRITE_ONLY);
    ssimPrepare_.set("uMode", mode == Mode::Reprojected ? 1 : 0);
    ssimPrepare_.set("uTonemap", tonemap ? 1 : 0);
    ssimPrepare_.set("uSize", static_cast<float>(w), static_cast<float>(h));
    ssimPrepare_.dispatch(w, h);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // Stage 2: separable Gaussian, horizontal then vertical.
    gaussian_.bind();
    gaussian_.set("uSize", w, h);
    for (int axis = 0; axis < 2; ++axis) {
        const int src = axis;
        const int dst = 1 - axis;
        stats0_[src].bindTexture(0);
        stats1_[src].bindTexture(1);
        stats0_[dst].bindImage(0, GL_WRITE_ONLY);
        stats1_[dst].bindImage(1, GL_WRITE_ONLY);
        gaussian_.set("uDir", axis == 0 ? 1 : 0, axis == 0 ? 0 : 1);
        gaussian_.dispatch(w, h);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    // Stage 3: evaluate and reduce. Two passes of ping-pong land the result
    // back in slot 0.
    const int groupsX = (w + kTile - 1) / kTile;
    const int groupsY = (h + kTile - 1) / kTile;
    const int groupCount = groupsX * groupsY;
    ensurePartials(groupCount);

    ssimReduce_.bind();
    stats0_[0].bindTexture(0);
    stats1_[0].bindTexture(1);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, partials_);
    ssimReduce_.set("uSize", w, h);
    glDispatchCompute(static_cast<GLuint>(groupsX), static_cast<GLuint>(groupsY), 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    std::vector<float> partials(static_cast<size_t>(groupCount) * 4);
    glGetNamedBufferSubData(partials_, 0, static_cast<GLsizeiptr>(partials.size() * sizeof(float)),
                            partials.data());

    double sumSsim = 0.0, windows = 0.0, worst = 1.0;
    for (int i = 0; i < groupCount; ++i) {
        sumSsim += partials[i * 4 + 0];
        if (partials[i * 4 + 3] > 0.0) worst = std::min(worst, static_cast<double>(partials[i * 4 + 2]));
        windows += partials[i * 4 + 3];
    }

    result.ssim = windows > 0.0 ? sumSsim / windows : 0.0;
    result.minSsim = windows > 0.0 ? worst : 0.0;
}

}  // namespace metrics
