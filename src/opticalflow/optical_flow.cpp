#include "opticalflow/optical_flow.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace opticalflow {

namespace {
constexpr int kSections = 9;
constexpr int kBins = 16;
constexpr int kHistogramSlots = 2;
constexpr int kHistogramUints = kHistogramSlots * kSections * kBins;

int divUp(int a, int b) { return (a + b - 1) / b; }
}  // namespace

void OpticalFlow::create() {
    luma_ = gfx::Program::compute("of_luma.comp");
    pyramid_program_ = gfx::Program::compute("of_pyramid.comp");
    histogram_ = gfx::Program::compute("of_histogram.comp");
    sceneChangeProgram_ = gfx::Program::compute("of_scene_change.comp");
    search_ = gfx::Program::compute("of_search.comp");
    filter_ = gfx::Program::compute("of_filter.comp");
    upscale_ = gfx::Program::compute("of_upscale.comp");
    epe_ = gfx::Program::compute("of_epe.comp");

    glCreateBuffers(1, &histogramBuffer_);
    glNamedBufferStorage(histogramBuffer_, kHistogramUints * sizeof(unsigned), nullptr,
                         GL_DYNAMIC_STORAGE_BIT);
    glCreateBuffers(1, &sceneChangeBuffer_);
    glNamedBufferStorage(sceneChangeBuffer_, 5 * sizeof(unsigned), nullptr, GL_DYNAMIC_STORAGE_BIT);
    // A ring so the read is always of a buffer the GPU finished with two frames
    // ago. Reading the live one would stall the pipeline every frame to deliver
    // a value nothing on the GPU side is waiting for.
    glCreateBuffers(3, sceneReadback_);
    for (int i = 0; i < 3; ++i)
        glNamedBufferStorage(sceneReadback_[i], 5 * sizeof(unsigned), nullptr, GL_MAP_READ_BIT);
}

void OpticalFlow::destroy() {
    luma_.destroy();
    pyramid_program_.destroy();
    histogram_.destroy();
    sceneChangeProgram_.destroy();
    search_.destroy();
    filter_.destroy();
    upscale_.destroy();
    epe_.destroy();
    for (int i = 0; i < 2; ++i) pyramid_[i].destroy();
    prevFlow_.destroy();
    for (int i = 0; i < kMaxLevels; ++i) {
        searchOut_[i].destroy();
        filtered_[i].destroy();
        predicted_[i].destroy();
    }
    if (histogramBuffer_) glDeleteBuffers(1, &histogramBuffer_);
    if (sceneChangeBuffer_) glDeleteBuffers(1, &sceneChangeBuffer_);
    glDeleteBuffers(3, sceneReadback_);
    if (epePartials_) glDeleteBuffers(1, &epePartials_);
    histogramBuffer_ = sceneChangeBuffer_ = epePartials_ = 0;
    sceneReadback_[0] = sceneReadback_[1] = sceneReadback_[2] = 0;
    epeCapacity_ = 0;
    levels_ = 0;
    historyValid_ = false;
}

void OpticalFlow::reloadShaders() {
    luma_.reloadIfChanged();
    pyramid_program_.reloadIfChanged();
    histogram_.reloadIfChanged();
    sceneChangeProgram_.reloadIfChanged();
    search_.reloadIfChanged();
    filter_.reloadIfChanged();
    upscale_.reloadIfChanged();
    epe_.reloadIfChanged();
}

void OpticalFlow::ensureResources(int displayWidth, int displayHeight) {
    // Half resolution, rounded up: losing the last column would shift every
    // block by a fraction of a texel at the right edge.
    const int lumaW = divUp(displayWidth, 2);
    const int lumaH = divUp(displayHeight, 2);

    // As many levels as fit while the coarsest still holds a full block.
    int levels = 1;
    while (levels < std::min(options_.levels, kMaxLevels) &&
           std::min(lumaW >> levels, lumaH >> levels) >= kBlockSize)
        ++levels;

    if (displayWidth == displayWidth_ && displayHeight == displayHeight_ && levels == levels_)
        return;

    displayWidth_ = displayWidth;
    displayHeight_ = displayHeight;
    levels_ = levels;

    for (int i = 0; i < 2; ++i) {
        pyramid_[i].ensure(lumaW, lumaH, GL_R16F, levels, "of.pyramid[" + std::to_string(i) + "]");
        pyramid_[i].clear();
    }
    historyValid_ = false;

    for (int l = 0; l < levels; ++l) {
        levelW_[l] = std::max(lumaW >> l, 1);
        levelH_[l] = std::max(lumaH >> l, 1);
        gridW_[l] = divUp(levelW_[l], kBlockSize);
        gridH_[l] = divUp(levelH_[l], kBlockSize);
        const std::string suffix = "[" + std::to_string(l) + "]";
        searchOut_[l].ensure(gridW_[l], gridH_[l], GL_RGBA16F, 1, "of.search" + suffix);
        filtered_[l].ensure(gridW_[l], gridH_[l], GL_RGBA16F, 1, "of.filtered" + suffix);
        predicted_[l].ensure(gridW_[l], gridH_[l], GL_RGBA16F, 1, "of.predicted" + suffix);
        searchOut_[l].clear();
        filtered_[l].clear();
        predicted_[l].clear();
    }
    prevFlow_.ensure(gridW_[0], gridH_[0], GL_RGBA16F, 1, "of.prevFlow");
    prevFlow_.clear();
}

void OpticalFlow::runSceneChange(gfx::GpuTimer& timer, bool reset) {
    gfx::GpuScope scope(timer, "OF scene change");

    const unsigned zero = 0;
    const int slot = histogramSlot_;
    glClearNamedBufferSubData(histogramBuffer_, GL_R32UI,
                              slot * kSections * kBins * sizeof(unsigned),
                              kSections * kBins * sizeof(unsigned), GL_RED_INTEGER,
                              GL_UNSIGNED_INT, &zero);

    histogram_.bind();
    pyramid_[pyramidIndex_].bindTexture(0);
    histogram_.set("uLuma", 0);
    histogram_.set("uLumaSize", pyramid_[0].width, pyramid_[0].height);
    histogram_.set("uSlot", slot);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, histogramBuffer_);
    histogram_.dispatch(pyramid_[0].width, pyramid_[0].height);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    sceneChangeProgram_.bind();
    sceneChangeProgram_.set("uSlot", slot);
    sceneChangeProgram_.set("uHasPrev", (historyValid_ && !reset) ? 1 : 0);
    sceneChangeProgram_.set("uThreshold", options_.sceneChangeThreshold);
    sceneChangeProgram_.set("uEnabled", options_.sceneChange ? 1 : 0);
    sceneChangeProgram_.set("uStatistic", options_.sceneChangeStatistic);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, histogramBuffer_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sceneChangeBuffer_);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    // Snapshot for the CPU, harvested two frames later.
    glCopyNamedBufferSubData(sceneChangeBuffer_, sceneReadback_[readbackIndex_], 0, 0,
                             5 * sizeof(unsigned));
    const int oldest = (readbackIndex_ + 1) % 3;
    unsigned data[5] = {0, 0, 0, 0, 0};
    glGetNamedBufferSubData(sceneReadback_[oldest], 0, sizeof(data), data);
    sceneChanged_ = data[0] != 0;
    std::memcpy(&sceneDifference_, &data[1], sizeof(float));
    std::memcpy(&sceneMax_, &data[2], sizeof(float));
    std::memcpy(&sceneMean_, &data[3], sizeof(float));
    std::memcpy(&sceneMedian_, &data[4], sizeof(float));
    readbackIndex_ = oldest;

    histogramSlot_ ^= 1;
}

void OpticalFlow::dispatch(const gfx::Texture2D& input, gfx::GpuTimer& timer, bool reset) {
    ensureResources(input.width, input.height);
    if (reset) historyValid_ = false;

    pyramidIndex_ ^= 1;
    gfx::Texture2D& cur = pyramid_[pyramidIndex_];
    gfx::Texture2D& prev = pyramid_[pyramidIndex_ ^ 1];

    gfx::GpuScope outer(timer, "Optical flow");

    {
        gfx::GpuScope scope(timer, "OF pyramid");
        luma_.bind();
        input.bindTexture(0);
        luma_.set("uInput", 0);
        cur.bindImage(0, GL_WRITE_ONLY, 0);
        luma_.set("uInputSize", input.width, input.height);
        luma_.set("uOutputSize", cur.width, cur.height);
        luma_.dispatch(cur.width, cur.height);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        pyramid_program_.bind();
        cur.bindTexture(0);
        pyramid_program_.set("uPyramid", 0);
        for (int l = 1; l < levels_; ++l) {
            cur.bindImage(0, GL_WRITE_ONLY, l);
            pyramid_program_.set("uSrcLevel", l - 1);
            pyramid_program_.set("uSrcSize", levelW_[l - 1], levelH_[l - 1]);
            pyramid_program_.set("uDstSize", levelW_[l], levelH_[l]);
            pyramid_program_.dispatch(levelW_[l], levelH_[l]);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    runSceneChange(timer, reset);

    {
        gfx::GpuScope scope(timer, "OF flow");
        for (int l = levels_ - 1; l >= 0; --l) {
            char label[32];
            std::snprintf(label, sizeof(label), "OF level %d", l);
            gfx::GpuScope levelScope(timer, label);

            // The prediction pass runs at every level, including the coarsest:
            // there it has no parent to inherit from, but the previous frame's
            // field and zero are still candidates worth testing.
            const bool hasCoarse = l < levels_ - 1;
            const bool temporal = options_.temporal && historyValid_;
            {
                upscale_.bind();
                filtered_[hasCoarse ? l + 1 : l].bindTexture(0);
                upscale_.set("uFlow", 0);
                cur.bindTexture(1);
                upscale_.set("uCur", 1);
                prev.bindTexture(2);
                upscale_.set("uPrev", 2);
                prevFlow_.bindTexture(3);
                upscale_.set("uPrevFlow", 3);
                predicted_[l].bindImage(0, GL_WRITE_ONLY);
                upscale_.set("uLevel", l);
                upscale_.set("uLevelSize", levelW_[l], levelH_[l]);
                upscale_.set("uFineGrid", gridW_[l], gridH_[l]);
                upscale_.set("uCoarseGrid", gridW_[hasCoarse ? l + 1 : l], gridH_[hasCoarse ? l + 1 : l]);
                upscale_.set("uHasCoarse", hasCoarse ? 1 : 0);
                upscale_.set("uEnabled", options_.upscale ? 1 : 0);
                upscale_.set("uTemporal", temporal ? 1 : 0);
                upscale_.set("uNovelty", hasCoarse ? options_.novelty : 0.f);
                // One workgroup per block, not per pixel: the pass reduces an
                // 8x8 SAD across its 64 threads.
                glDispatchCompute(gridW_[l], gridH_[l], 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
            }

            search_.bind();
            cur.bindTexture(0);
            search_.set("uCur", 0);
            prev.bindTexture(1);
            search_.set("uPrev", 1);
            predicted_[l].bindTexture(2);
            search_.set("uPredicted", 2);
            searchOut_[l].bindImage(0, GL_WRITE_ONLY);
            search_.set("uLevel", l);
            search_.set("uLevelSize", levelW_[l], levelH_[l]);
            search_.set("uBlockGrid", gridW_[l], gridH_[l]);
            search_.set("uRadius", options_.radius);
            search_.set("uHasPredicted", 1);
            search_.set("uSmoothness", options_.smoothness);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sceneChangeBuffer_);
            glDispatchCompute(gridW_[l], gridH_[l], 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

            filter_.bind();
            searchOut_[l].bindTexture(0);
            filter_.set("uFlow", 0);
            filtered_[l].bindImage(0, GL_WRITE_ONLY);
            filter_.set("uBlockGrid", gridW_[l], gridH_[l]);
            filter_.set("uLevelSize", levelW_[l], levelH_[l]);
            filter_.set("uEnabled", options_.filter ? 1 : 0);
            filter_.dispatch(gridW_[l], gridH_[l]);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    // Snapshot the finished field for next frame's temporal candidate.
    glCopyImageSubData(filtered_[0].id, GL_TEXTURE_2D, 0, 0, 0, 0, prevFlow_.id, GL_TEXTURE_2D, 0,
                       0, 0, 0, gridW_[0], gridH_[0], 1);

    historyValid_ = true;
}

Accuracy OpticalFlow::validate(const gfx::Texture2D& velocity, int displayWidth,
                               int displayHeight) {
    Accuracy result;
    if (!valid()) return result;

    const int groupsX = divUp(gridW_[0], 8);
    const int groupsY = divUp(gridH_[0], 8);
    const int groups = groupsX * groupsY;
    if (groups > epeCapacity_) {
        if (epePartials_) glDeleteBuffers(1, &epePartials_);
        glCreateBuffers(1, &epePartials_);
        glNamedBufferStorage(epePartials_, groups * 4 * sizeof(float), nullptr, GL_MAP_READ_BIT);
        epeCapacity_ = groups;
    }

    epe_.bind();
    filtered_[0].bindTexture(0);
    epe_.set("uFlow", 0);
    velocity.bindTexture(1);
    epe_.set("uVelocity", 1);
    epe_.set("uBlockGrid", gridW_[0], gridH_[0]);
    epe_.set("uDisplaySize", static_cast<float>(displayWidth), static_cast<float>(displayHeight));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, epePartials_);
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    std::vector<float> partials(groups * 4);
    glGetNamedBufferSubData(epePartials_, 0, groups * 4 * sizeof(float), partials.data());

    double sum = 0.0, count = 0.0, w1 = 0.0, w2 = 0.0;
    for (int i = 0; i < groups; ++i) {
        sum += partials[i * 4 + 0];
        count += partials[i * 4 + 1];
        w1 += partials[i * 4 + 2];
        w2 += partials[i * 4 + 3];
    }
    if (count > 0.0) {
        result.meanEpe = sum / count;
        result.within1 = w1 / count;
        result.within2 = w2 / count;
        result.blocks = count;
    }
    return result;
}

}  // namespace opticalflow
