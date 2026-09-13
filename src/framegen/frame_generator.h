#pragma once

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "framegen/ml_blend.h"

namespace framegen {

// Levels of the vector field inpainting pyramid. Seven halvings of a 1280-wide
// field leave a 10x6 top level, which is coarse enough that any hole at all is
// covered and fine enough that the vector it hands back still describes the
// right part of the image.
constexpr int kMaxLevels = 7;

struct Options {
    // Relative depth agreement used both for the primary/secondary decision in
    // the scatter and for the two disocclusion masks.
    float depthTolerance = 0.02f;
    // Use the upscaler's dilated motion vectors when it produced them. Off
    // falls back to the raw velocity buffer, which is the ablation that shows
    // what dilation is worth on a silhouette.
    bool dilate = true;
    bool gameField = true;
    bool flowField = true;
    bool masks = true;
    // 0 turns the inpainting pyramid off: a hole in the field stays a hole and
    // the pixel falls back to the blend.
    int levels = kMaxLevels;
    // Which child survives the pyramid reduction: 0 = highest priority (the
    // nearest surface), 1 = lowest (the background). See fg_field_pyramid.comp.
    int inpaintPick = 1;
    // Colour agreement as the low five bits of the scatter priority. Off by
    // default, measured: identical PSNR and SSIM to the second decimal on both
    // scenes, for 0.37 ms at 1080p -- it is the only part of the two scatter
    // passes that reads display-resolution images, and it only ever breaks
    // ties the primary bit and depth have already settled.
    bool colorPriority = false;
    // Weight multiplier on the flow candidate wherever a game vector exists.
    // See fg_interpolate.comp for why colour agreement alone cannot be trusted
    // to arbitrate between the two on periodic texture. Measured optimum of
    // 1 / 0.5 / 0.25 / 0.1: +0.15 dB on Sponza, -0.02 dB on the procedural
    // scene; lower keeps gaining on Sponza's mean but starts losing its worst
    // frame and the procedural scene's mean.
    float flowBias = 0.5f;
    // Colour distance that costs a candidate a factor of e of its weight.
    float agreement = 24.f;
    // Flow match error above which a block counts as a failed match, in the
    // flow's own units (mean absolute luminance difference).
    float flowErrorThreshold = 0.05f;
    // Displacement in display pixels that saturates the flow field's magnitude
    // priority.
    float flowMagnitudeScale = 64.f;
    // Also produce the 50/50 blend of the same two frames, so the measurement
    // path can score the module against the cost of doing nothing. Off by
    // default: it is a display-resolution pass that the pipeline itself does
    // not need.
    bool measureBlend = false;

    // M8, passes 8-9: fill pixels the warps could not cover from a
    // coverage-weighted pyramid of the interpolated image itself.
    bool imageInpaint = true;
    // Reject warp samples whose position is off screen (see fg_interpolate.comp).
    bool boundsCheck = true;
    // Mean coverage a pyramid texel needs before pass 9 uses its colour.
    float inpaintMinCoverage = 0.3f;
    // Count mask-rejected sides against coverage (see fg_interpolate.comp).
    bool coverageMasks = true;

    // M9, passes 10-12: weights of the learned blend (src/ml/blend_net.h).
    // Empty keeps the heuristic frame. When set, output() is the network's
    // frame and heuristicOutput() still holds the one it was given.
    std::string mlWeights;
    // Compute the network's features and candidates every frame so the
    // dataset capture can read them back.
    bool mlDump = false;
};

// Everything pass 2 through 7 read, gathered per frame.
struct Inputs {
    // Display resolution, display-referred: the two frames the interpolated
    // one goes between. `previous` is kept by the module itself; the caller
    // only supplies the current one.
    const gfx::Texture2D* current = nullptr;

    // Render resolution. `dilated` is the upscaler's (xy = vector,
    // z = disocclusion, w = nearest depth) and may be null.
    const gfx::Texture2D* dilated = nullptr;
    const gfx::Texture2D* velocity = nullptr;
    const gfx::Texture2D* depthInfo = nullptr;
    const gfx::Texture2D* prevDepthInfo = nullptr;
    int renderWidth = 0;
    int renderHeight = 0;

    // M6's field: one vector per 8x8 luma block, same UV convention. Null when
    // optical flow is not running, in which case the module runs on game
    // vectors alone.
    const gfx::Texture2D* flow = nullptr;

    // First frame, resize, or a camera cut: nothing to interpolate between.
    bool reset = false;
};

// Module D: frame generation, passes 1-9.
//
// Produces the frame at t-0.5 from the two display-resolution frames around it,
// the game's motion vectors and the optical flow field. The vectors are in UV
// space and point current -> previous, so the interpolated frame sits at half
// a vector from each side -- that single convention is what keeps the whole
// module free of direction bugs.
//
// The module owns the previous frame: the caller hands it this frame's image
// every frame and gets back the interpolated one, which by construction lags
// the input by half a frame. Scheduling that for display is M8's problem.
class FrameGenerator {
public:
    void create();
    void destroy();
    void reloadShaders();

    void setOptions(const Options& options) { options_ = options; }
    const Options& options() const { return options_; }

    // Runs passes 1-9 (1-12 with the learned blend). Returns the interpolated frame, which is also what
    // output() gives until the next call. The first call after create() or a
    // reset produces a copy of `current`.
    const gfx::Texture2D& dispatch(const Inputs& inputs, gfx::GpuTimer& timer);

    const gfx::Texture2D& output() const { return mlValid_ ? ml_.output() : output_; }
    // Passes 7-9 only, whatever the learned blend did with them.
    const gfx::Texture2D& heuristicOutput() const { return output_; }
    MlBlend& ml() { return ml_; }
    // 50/50 blend of the same two frames; only filled when measureBlend is set.
    const gfx::Texture2D& blend() const { return blend_; }
    // Display resolution, rgb = (occlusion vs previous, occlusion vs current,
    // fraction of the pyramid the vector had to be inpainted from).
    const gfx::Texture2D& debug() const { return debug_; }
    bool valid() const { return output_.valid() && historyValid_; }

    // Resolved vector fields, for the debug views and the documentation
    // figures: xy = vector, z = priority, w = validity.
    const gfx::Texture2D& gameField() const { return gameField_; }
    const gfx::Texture2D& flowField() const { return flowField_; }
    int fieldWidth() const { return fieldW_; }
    int fieldHeight() const { return fieldH_; }

private:
    void ensureResources(int displayWidth, int displayHeight, int renderWidth, int renderHeight);
    void scatterFields(const Inputs& inputs, gfx::GpuTimer& timer);
    void resolveField(const gfx::Texture2D& fieldX, const gfx::Texture2D& fieldY,
                      gfx::Texture2D& resolved);
    void buildPyramid(gfx::Texture2D& resolved);
    void inpaintImage(gfx::GpuTimer& timer);

    gfx::Program setup_;
    gfx::Program depth_;
    gfx::Program gameFieldProgram_;
    gfx::Program flowFieldProgram_;
    gfx::Program resolve_;
    gfx::Program pyramid_;
    gfx::Program disocclusion_;
    gfx::Program interpolate_;
    gfx::Program blendProgram_;
    gfx::Program inpaintPyramid_;
    gfx::Program inpaint_;

    // Field resolution = render resolution: the scatter has one source pixel
    // per texel there, which is the density at which a scattered field is
    // neither wastefully sparse nor self-occluding.
    gfx::Texture2D depthField_;   // R32UI, nearest linear distance at t-0.5
    gfx::Texture2D gameX_, gameY_;
    gfx::Texture2D flowX_, flowY_;
    gfx::Texture2D gameField_;    // RGBA16F + mips
    gfx::Texture2D flowField_;
    gfx::Texture2D masks_;        // RGBA8, xy = the two masks
    gfx::Texture2D prevColor_;    // display resolution, last frame's image
    gfx::Texture2D output_;
    gfx::Texture2D blend_;
    gfx::Texture2D debug_;
    // Half display resolution and down, premultiplied colour + mean coverage.
    gfx::Texture2D imagePyramid_;

    MlBlend ml_;
    bool mlValid_ = false;

    Options options_;
    int displayWidth_ = 0;
    int displayHeight_ = 0;
    int fieldW_ = 0;
    int fieldH_ = 0;
    int levels_ = 0;
    bool historyValid_ = false;
};

}  // namespace framegen
