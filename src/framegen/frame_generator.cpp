#include "framegen/frame_generator.h"

#include <algorithm>
#include <string>

namespace framegen {

namespace {
void barrier() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}
}  // namespace

void FrameGenerator::create() {
    setup_ = gfx::Program::compute("fg_setup.comp");
    depth_ = gfx::Program::compute("fg_depth.comp");
    gameFieldProgram_ = gfx::Program::compute("fg_game_field.comp");
    flowFieldProgram_ = gfx::Program::compute("fg_flow_field.comp");
    resolve_ = gfx::Program::compute("fg_resolve_field.comp");
    pyramid_ = gfx::Program::compute("fg_field_pyramid.comp");
    disocclusion_ = gfx::Program::compute("fg_disocclusion.comp");
    interpolate_ = gfx::Program::compute("fg_interpolate.comp");
    blendProgram_ = gfx::Program::compute("fg_blend.comp");
    inpaintPyramid_ = gfx::Program::compute("fg_inpaint_pyramid.comp");
    inpaint_ = gfx::Program::compute("fg_inpaint.comp");
    if (!options_.mlWeights.empty() || options_.mlDump) ml_.create(options_.mlWeights, options_.mlDump);
}

void FrameGenerator::destroy() {
    setup_.destroy();
    depth_.destroy();
    gameFieldProgram_.destroy();
    flowFieldProgram_.destroy();
    resolve_.destroy();
    pyramid_.destroy();
    disocclusion_.destroy();
    interpolate_.destroy();
    blendProgram_.destroy();
    inpaintPyramid_.destroy();
    inpaint_.destroy();
    ml_.destroy();
    mlValid_ = false;
    imagePyramid_.destroy();
    depthField_.destroy();
    gameX_.destroy();
    gameY_.destroy();
    flowX_.destroy();
    flowY_.destroy();
    gameField_.destroy();
    flowField_.destroy();
    masks_.destroy();
    prevColor_.destroy();
    output_.destroy();
    blend_.destroy();
    debug_.destroy();
    displayWidth_ = displayHeight_ = fieldW_ = fieldH_ = levels_ = 0;
    historyValid_ = false;
}

void FrameGenerator::reloadShaders() {
    setup_.reloadIfChanged();
    depth_.reloadIfChanged();
    gameFieldProgram_.reloadIfChanged();
    flowFieldProgram_.reloadIfChanged();
    resolve_.reloadIfChanged();
    pyramid_.reloadIfChanged();
    disocclusion_.reloadIfChanged();
    interpolate_.reloadIfChanged();
    blendProgram_.reloadIfChanged();
    inpaintPyramid_.reloadIfChanged();
    inpaint_.reloadIfChanged();
    ml_.reloadShaders();
}

void FrameGenerator::ensureResources(int displayWidth, int displayHeight, int renderWidth,
                                     int renderHeight) {
    int levels = 1;
    const int wanted = std::clamp(options_.levels, 1, kMaxLevels);
    while (levels < wanted && std::min(renderWidth >> levels, renderHeight >> levels) >= 1) ++levels;

    if (displayWidth == displayWidth_ && displayHeight == displayHeight_ &&
        renderWidth == fieldW_ && renderHeight == fieldH_ && levels == levels_)
        return;

    displayWidth_ = displayWidth;
    displayHeight_ = displayHeight;
    fieldW_ = renderWidth;
    fieldH_ = renderHeight;
    levels_ = levels;

    depthField_.ensure(fieldW_, fieldH_, GL_R32UI, 1, "fg.depthField");
    gameX_.ensure(fieldW_, fieldH_, GL_R32UI, 1, "fg.gameField.x");
    gameY_.ensure(fieldW_, fieldH_, GL_R32UI, 1, "fg.gameField.y");
    flowX_.ensure(fieldW_, fieldH_, GL_R32UI, 1, "fg.flowField.x");
    flowY_.ensure(fieldW_, fieldH_, GL_R32UI, 1, "fg.flowField.y");
    gameField_.ensure(fieldW_, fieldH_, GL_RGBA16F, levels_, "fg.gameField");
    flowField_.ensure(fieldW_, fieldH_, GL_RGBA16F, levels_, "fg.flowField");
    gameField_.clear();
    flowField_.clear();
    masks_.ensure(fieldW_, fieldH_, GL_RGBA8, 1, "fg.masks");
    masks_.clear();

    prevColor_.ensure(displayWidth_, displayHeight_, GL_RGBA16F, 1, "fg.prevColor");
    prevColor_.clear();
    output_.ensure(displayWidth_, displayHeight_, GL_RGBA16F, 1, "fg.output");
    blend_.ensure(displayWidth_, displayHeight_, GL_RGBA16F, 1, "fg.blend");
    debug_.ensure(displayWidth_, displayHeight_, GL_RGBA8, 1, "fg.debug");
    {
        // Down to a top level a few texels across: a hole the size of the
        // frame edge strip a fast pan uncovers still finds covered pixels.
        const int w = std::max(displayWidth_ / 2, 1);
        const int h = std::max(displayHeight_ / 2, 1);
        int pyramidLevels = 1;
        while (std::min(w >> pyramidLevels, h >> pyramidLevels) >= 4) ++pyramidLevels;
        imagePyramid_.ensure(w, h, GL_RGBA16F, pyramidLevels, "fg.imagePyramid");
    }
    historyValid_ = false;
}

void FrameGenerator::resolveField(const gfx::Texture2D& fieldX, const gfx::Texture2D& fieldY,
                                  gfx::Texture2D& resolved) {
    resolve_.bind();
    fieldX.bindTexture(0);
    resolve_.set("uFieldX", 0);
    fieldY.bindTexture(1);
    resolve_.set("uFieldY", 1);
    resolved.bindImage(0, GL_WRITE_ONLY, 0);
    resolve_.set("uFieldSize", fieldW_, fieldH_);
    resolve_.dispatch(fieldW_, fieldH_);
    barrier();
}

void FrameGenerator::buildPyramid(gfx::Texture2D& resolved) {
    if (levels_ <= 1) return;
    pyramid_.bind();
    resolved.bindTexture(0);
    pyramid_.set("uField", 0);
    pyramid_.set("uPick", options_.inpaintPick);
    for (int l = 1; l < levels_; ++l) {
        const int srcW = std::max(fieldW_ >> (l - 1), 1);
        const int srcH = std::max(fieldH_ >> (l - 1), 1);
        const int dstW = std::max(fieldW_ >> l, 1);
        const int dstH = std::max(fieldH_ >> l, 1);
        resolved.bindImage(0, GL_WRITE_ONLY, l);
        pyramid_.set("uSrcLevel", l - 1);
        pyramid_.set("uSrcSize", srcW, srcH);
        pyramid_.set("uDstSize", dstW, dstH);
        pyramid_.dispatch(dstW, dstH);
        barrier();
    }
}

void FrameGenerator::inpaintImage(gfx::GpuTimer& timer) {
    gfx::GpuScope scope(timer, "FG image inpaint");
    // Pass 8: the pyramid. Level 0 is reduced straight from the output.
    inpaintPyramid_.bind();
    inpaintPyramid_.set("uSource", 0);
    for (int l = 0; l < imagePyramid_.levels; ++l) {
        const bool first = l == 0;
        const int srcW = first ? displayWidth_ : std::max(imagePyramid_.width >> (l - 1), 1);
        const int srcH = first ? displayHeight_ : std::max(imagePyramid_.height >> (l - 1), 1);
        const int dstW = std::max(imagePyramid_.width >> l, 1);
        const int dstH = std::max(imagePyramid_.height >> l, 1);
        if (first)
            output_.bindTexture(0);
        else
            imagePyramid_.bindTexture(0);
        imagePyramid_.bindImage(0, GL_WRITE_ONLY, l);
        inpaintPyramid_.set("uFirst", first ? 1 : 0);
        inpaintPyramid_.set("uSrcLevel", first ? 0 : l - 1);
        inpaintPyramid_.set("uSrcSize", srcW, srcH);
        inpaintPyramid_.set("uDstSize", dstW, dstH);
        inpaintPyramid_.dispatch(dstW, dstH);
        barrier();
    }

    // Pass 9: fill in place.
    inpaint_.bind();
    imagePyramid_.bindTexture(0);
    inpaint_.set("uPyramid", 0);
    output_.bindImage(0, GL_READ_WRITE);
    inpaint_.set("uDisplaySize", displayWidth_, displayHeight_);
    inpaint_.set("uLevels", imagePyramid_.levels);
    inpaint_.set("uMinCoverage", options_.inpaintMinCoverage);
    inpaint_.dispatch(displayWidth_, displayHeight_);
    barrier();
}

void FrameGenerator::scatterFields(const Inputs& in, gfx::GpuTimer& timer) {
    const bool hasDilated = options_.dilate && in.dilated != nullptr && in.dilated->valid();
    // The fallback path still needs both textures bound, because a sampler a
    // shader never reads must still be a complete texture object.
    const gfx::Texture2D& dilated = hasDilated ? *in.dilated : *in.velocity;

    {
        gfx::GpuScope scope(timer, "FG depth");
        depth_.bind();
        dilated.bindTexture(0);
        depth_.set("uDilated", 0);
        in.velocity->bindTexture(1);
        depth_.set("uVelocity", 1);
        in.depthInfo->bindTexture(2);
        depth_.set("uDepthInfo", 2);
        depthField_.bindImage(0, GL_READ_WRITE);
        depth_.set("uRenderSize", static_cast<float>(in.renderWidth),
                   static_cast<float>(in.renderHeight));
        depth_.set("uFieldSize", static_cast<float>(fieldW_), static_cast<float>(fieldH_));
        depth_.set("uHasDilated", hasDilated ? 1 : 0);
        depth_.dispatch(in.renderWidth, in.renderHeight);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    if (options_.gameField) {
        gfx::GpuScope scope(timer, "FG game vector field");
        gameFieldProgram_.bind();
        dilated.bindTexture(0);
        gameFieldProgram_.set("uDilated", 0);
        in.velocity->bindTexture(1);
        gameFieldProgram_.set("uVelocity", 1);
        in.depthInfo->bindTexture(2);
        gameFieldProgram_.set("uDepthInfo", 2);
        in.current->bindTexture(3);
        gameFieldProgram_.set("uCurrent", 3);
        prevColor_.bindTexture(4);
        gameFieldProgram_.set("uPrevious", 4);
        depthField_.bindImage(0, GL_READ_ONLY);
        gameX_.bindImage(1, GL_READ_WRITE);
        gameY_.bindImage(2, GL_READ_WRITE);
        gameFieldProgram_.set("uRenderSize", static_cast<float>(in.renderWidth),
                              static_cast<float>(in.renderHeight));
        gameFieldProgram_.set("uFieldSize", static_cast<float>(fieldW_),
                              static_cast<float>(fieldH_));
        gameFieldProgram_.set("uHasDilated", hasDilated ? 1 : 0);
        gameFieldProgram_.set("uDepthTolerance", options_.depthTolerance);
        gameFieldProgram_.set("uColorPriority", options_.colorPriority ? 1 : 0);
        gameFieldProgram_.dispatch(in.renderWidth, in.renderHeight);
        barrier();
        resolveField(gameX_, gameY_, gameField_);
        buildPyramid(gameField_);
    }

    const bool useFlow = options_.flowField && in.flow != nullptr && in.flow->valid();
    if (useFlow) {
        gfx::GpuScope scope(timer, "FG flow vector field");
        flowFieldProgram_.bind();
        in.flow->bindTexture(0);
        flowFieldProgram_.set("uFlow", 0);
        in.current->bindTexture(1);
        flowFieldProgram_.set("uCurrent", 1);
        prevColor_.bindTexture(2);
        flowFieldProgram_.set("uPrevious", 2);
        flowX_.bindImage(1, GL_READ_WRITE);
        flowY_.bindImage(2, GL_READ_WRITE);
        flowFieldProgram_.set("uFieldSize", static_cast<float>(fieldW_),
                              static_cast<float>(fieldH_));
        flowFieldProgram_.set("uDisplaySize", static_cast<float>(displayWidth_),
                              static_cast<float>(displayHeight_));
        flowFieldProgram_.set("uErrorThreshold", options_.flowErrorThreshold);
        flowFieldProgram_.set("uMagnitudeScale", options_.flowMagnitudeScale);
        flowFieldProgram_.set("uColorPriority", options_.colorPriority ? 1 : 0);
        flowFieldProgram_.dispatch(fieldW_, fieldH_);
        barrier();
        resolveField(flowX_, flowY_, flowField_);
        buildPyramid(flowField_);
    }

    if (options_.masks) {
        gfx::GpuScope scope(timer, "FG disocclusion masks");
        disocclusion_.bind();
        gameField_.bindTexture(0);
        disocclusion_.set("uField", 0);
        in.depthInfo->bindTexture(1);
        disocclusion_.set("uDepthInfo", 1);
        in.prevDepthInfo->bindTexture(2);
        disocclusion_.set("uPrevDepthInfo", 2);
        depthField_.bindImage(0, GL_READ_ONLY);
        masks_.bindImage(1, GL_WRITE_ONLY);
        disocclusion_.set("uFieldSize", fieldW_, fieldH_);
        disocclusion_.set("uDepthTolerance", options_.depthTolerance);
        disocclusion_.set("uEnabled", options_.gameField ? 1 : 0);
        disocclusion_.dispatch(fieldW_, fieldH_);
        barrier();
    }
}

const gfx::Texture2D& FrameGenerator::dispatch(const Inputs& in, gfx::GpuTimer& timer) {
    ensureResources(in.current->width, in.current->height, in.renderWidth, in.renderHeight);
    const bool reset = in.reset || !historyValid_;

    // Runs before the module's own scope opens, and before this frame's image
    // replaces the stored previous one: the baseline has to see exactly the
    // same pair the interpolation sees.
    if (options_.measureBlend && !reset) {
        // Named as a reference pass: it is timed and reported but must not
        // count towards the cost of the pipeline, which never runs it.
        gfx::GpuScope scope(timer, "ref: FG blend");
        blendProgram_.bind();
        in.current->bindTexture(0);
        blendProgram_.set("uCurrent", 0);
        prevColor_.bindTexture(1);
        blendProgram_.set("uPrevious", 1);
        blend_.bindImage(0, GL_WRITE_ONLY);
        blendProgram_.set("uDisplaySize", static_cast<float>(displayWidth_),
                          static_cast<float>(displayHeight_));
        blendProgram_.dispatch(displayWidth_, displayHeight_);
        barrier();
    }

    {
        gfx::GpuScope outer(timer, "Frame generation");

        // Pass 1, setup. The scatter targets are accumulators, so every frame
        // starts from "nothing here": 0 is an impossible priority and 0xFFFFFFFF
        // an impossible depth, which is what lets the later passes tell an empty
        // texel from a real sample without a separate coverage image.
        if (!reset) {
            {
                gfx::GpuScope scope(timer, "FG setup");
                setup_.bind();
                depthField_.bindImage(0, GL_WRITE_ONLY);
                gameX_.bindImage(1, GL_WRITE_ONLY);
                gameY_.bindImage(2, GL_WRITE_ONLY);
                flowX_.bindImage(3, GL_WRITE_ONLY);
                flowY_.bindImage(4, GL_WRITE_ONLY);
                setup_.set("uFieldSize", fieldW_, fieldH_);
                setup_.dispatch(fieldW_, fieldH_);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }
            scatterFields(in, timer);
        }

        const bool useFlow = options_.flowField && in.flow != nullptr && in.flow->valid();
        {
            gfx::GpuScope scope(timer, "FG interpolate");
            interpolate_.bind();
            in.current->bindTexture(0);
            interpolate_.set("uCurrent", 0);
            prevColor_.bindTexture(1);
            interpolate_.set("uPrevious", 1);
            gameField_.bindTexture(2);
            interpolate_.set("uGameField", 2);
            flowField_.bindTexture(3);
            interpolate_.set("uFlowField", 3);
            masks_.bindTexture(4);
            interpolate_.set("uMasks", 4);
            output_.bindImage(0, GL_WRITE_ONLY);
            debug_.bindImage(1, GL_WRITE_ONLY);
            interpolate_.set("uDisplaySize", static_cast<float>(displayWidth_),
                             static_cast<float>(displayHeight_));
            interpolate_.set("uFieldSize", fieldW_, fieldH_);
            interpolate_.set("uLevels", levels_);
            interpolate_.set("uGameEnabled", options_.gameField ? 1 : 0);
            interpolate_.set("uFlowEnabled", useFlow ? 1 : 0);
            interpolate_.set("uMaskEnabled", options_.masks && options_.gameField ? 1 : 0);
            interpolate_.set("uAgreement", options_.agreement);
            interpolate_.set("uFlowBias", options_.flowBias);
            interpolate_.set("uReset", reset ? 1 : 0);
            interpolate_.set("uBoundsCheck", options_.boundsCheck ? 1 : 0);
            interpolate_.set("uCoverageMasks", options_.coverageMasks ? 1 : 0);
            interpolate_.dispatch(displayWidth_, displayHeight_);
            barrier();
        }

        // A reset frame is a copy, fully covered: nothing to fill.
        if (options_.imageInpaint && !reset) inpaintImage(timer);

        // M9: the learned blend reads everything above, including the
        // previous frame, so it runs before that is overwritten.
        mlValid_ = false;
        if (ml_.enabled() && !reset) {
            MlInputs ml;
            ml.current = in.current;
            ml.previous = &prevColor_;
            ml.gameField = &gameField_;
            ml.flowField = &flowField_;
            ml.masks = &masks_;
            ml.heuristic = &output_;
            ml.fieldWidth = fieldW_;
            ml.fieldHeight = fieldH_;
            ml.levels = levels_;
            ml.game = options_.gameField;
            ml.flow = useFlow;
            ml.masksEnabled = options_.masks && options_.gameField;
            mlValid_ = ml_.dispatch(ml, timer);
        }

        // Kept inside the module's scope: the pipeline needs this frame as next
        // frame's "previous", so the copy is part of its cost.
        glCopyImageSubData(in.current->id, GL_TEXTURE_2D, 0, 0, 0, 0, prevColor_.id, GL_TEXTURE_2D, 0,
                           0, 0, 0, displayWidth_, displayHeight_, 1);
    }

    historyValid_ = true;
    return output();
}

}  // namespace framegen
