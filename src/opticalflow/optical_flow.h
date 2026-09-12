#pragma once

#include <string>

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"

namespace opticalflow {

// Upper bound on pyramid levels. 7 levels over a half-resolution luma image
// means the coarsest search sees the whole frame at 1/128 scale, which covers
// any displacement a real camera can produce between two frames.
constexpr int kMaxLevels = 7;
// Block matching granularity, in luma texels. Shared by every level; the
// search shader has it as a compile-time constant because the workgroup shape
// is derived from it.
constexpr int kBlockSize = 8;

struct Options {
    int levels = kMaxLevels;
    // Search radius in texels of the level being searched. The reference
    // design uses a wider window at the top; here every level is the same and
    // the pyramid supplies the reach, which is measured in docs/OPTICALFLOW.md.
    int radius = 4;
    // Penalty per texel of distance from the coarser level's prediction, in
    // units of mean absolute luminance -- the same units the SAD is reported
    // in, which is what makes the number small. A good match on this content
    // scores around 0.02, so a penalty of 0.01 per texel is the size of the
    // entire signal and pins the search to its prediction; measured, that is
    // the difference between 1.16 px and 39 px of endpoint error. See
    // docs/OPTICALFLOW.md.
    float smoothness = 0.0005f;
    bool filter = true;
    bool upscale = true;
    // Offer the previous frame's finished field as a candidate at every level.
    bool temporal = true;
    // Surcharge added to the two candidates that are not inherited from the
    // coarser level -- last frame's field and zero -- everywhere except the
    // coarsest level, where there is nothing to inherit and they are the only
    // way in. Inside a low-contrast block every candidate scores within a
    // thousandth of every other, so without a surcharge these two win on noise
    // and discard a vector the pyramid had already got right. The SAD is a
    // mean absolute luminance difference and therefore at most 1, so a novelty
    // of 1 is a hard exclusion rather than a penalty; that is the measured
    // optimum at every speed tested (dt=1/60: 1.16 px against 1.47 px at
    // 0.004, 2.07 px at 0). The knob is kept as a float because the sweep
    // across it is what shows the effect. See docs/OPTICALFLOW.md.
    float novelty = 0.001f;
    bool sceneChange = true;
    // Section distance above which the frame is called a cut. On the
    // measurement orbit the largest value a continuous camera produces is
    // 0.159 and the smallest a real cut produces is 0.343 (--cut-every), so
    // the threshold sits between the two. See docs/OPTICALFLOW.md for the
    // pooled table, including the one orbit where the two ranges touch.
    float sceneChangeThreshold = 0.25f;
    // Which of the nine per-section distances the verdict is taken from:
    // 0 = worst, 1 = mean, 2 = median. Measured, the mean separates cuts from
    // fast camera motion by the widest margin: a pan replaces the content of
    // the two or three sections at the leading edge outright, which the
    // maximum cannot tell from a cut, while a cut to visually similar content
    // -- half an orbit away inside the same stone atrium -- moves only a few
    // sections far, which the median cannot tell from a pan.
    int sceneChangeStatistic = 1;
};

// Result of scoring the flow field against the game's motion vectors. Only
// meaningful while the scene's motion is camera motion, which is exactly the
// case the rasteriser's vectors describe exactly.
struct Accuracy {
    double meanEpe = 0.0;   // display pixels
    double within1 = 0.0;   // fraction of blocks
    double within2 = 0.0;
    double blocks = 0.0;
};

// Module C: pyramidal block-matching optical flow.
//
// Runs on the final display-resolution image and produces one motion vector
// per 8x8 block of a half-resolution luminance image -- so one vector per 16x16
// display pixels. Vectors are in UV space and point from this frame to the
// previous one, the same convention the G-buffer's velocity buffer uses, so
// the two are directly comparable and directly interchangeable.
class OpticalFlow {
public:
    void create();
    void destroy();
    void reloadShaders();

    void setOptions(const Options& options) { options_ = options; }
    const Options& options() const { return options_; }

    // `input` is display resolution and display-referred. `reset` discards the
    // previous frame's pyramid, which is what a camera cut or a resize needs.
    void dispatch(const gfx::Texture2D& input, gfx::GpuTimer& timer, bool reset);

    // One vector per block, RGBA16F: xy = UV displacement current -> previous,
    // z = the mean absolute luminance difference of the winning match (0 is a
    // perfect match), w = 1 where a vector was estimated at all.
    const gfx::Texture2D& flow() const { return filtered_[0]; }
    int blockGridWidth() const { return gridW_[0]; }
    int blockGridHeight() const { return gridH_[0]; }
    bool valid() const { return filtered_[0].valid(); }

    // The luminance pyramid, for the debug views.
    const gfx::Texture2D& pyramid() const { return pyramid_[pyramidIndex_]; }
    int levels() const { return levels_; }

    // Scores this frame's flow against the velocity buffer. Synchronous: it
    // reads the reduction back, so it belongs in the validation path only.
    Accuracy validate(const gfx::Texture2D& velocity, int displayWidth, int displayHeight);

    // The worst section's histogram distance, and the verdict derived from it.
    // Read back from the GPU with a two-frame delay so no frame ever stalls on
    // it; the shaders themselves consume the value directly and are not
    // delayed.
    float sceneChangeDifference() const { return sceneDifference_; }
    // The three candidate statistics over the nine sections, all measured every
    // frame so the choice between them can be made from data rather than
    // asserted. See docs/OPTICALFLOW.md.
    float sceneChangeMax() const { return sceneMax_; }
    float sceneChangeMean() const { return sceneMean_; }
    float sceneChangeMedian() const { return sceneMedian_; }
    bool sceneChanged() const { return sceneChanged_; }

private:
    void ensureResources(int displayWidth, int displayHeight);
    void runSceneChange(gfx::GpuTimer& timer, bool reset);

    gfx::Program luma_;
    gfx::Program pyramid_program_;
    gfx::Program histogram_;
    gfx::Program sceneChangeProgram_;
    gfx::Program search_;
    gfx::Program filter_;
    gfx::Program upscale_;
    gfx::Program epe_;

    // Half-resolution luminance with a mip chain, this frame and last.
    gfx::Texture2D pyramid_[2];
    int pyramidIndex_ = 0;
    bool historyValid_ = false;

    // Per level: raw search output, median-filtered output, and the prediction
    // handed down from the level above. Kept apart rather than ping-ponged
    // because every one of them is a debug view worth having, and together
    // they add up to under a megabyte.
    gfx::Texture2D searchOut_[kMaxLevels];
    gfx::Texture2D filtered_[kMaxLevels];
    gfx::Texture2D predicted_[kMaxLevels];
    // Last frame's finished field. A copy rather than a ping-pong slot: it is
    // 65 kB, and keeping filtered_[0] as the one place the result lives keeps
    // every consumer and every debug view pointed at a single texture.
    gfx::Texture2D prevFlow_;
    int levelW_[kMaxLevels] = {};
    int levelH_[kMaxLevels] = {};
    int gridW_[kMaxLevels] = {};
    int gridH_[kMaxLevels] = {};
    int levels_ = 0;

    GLuint histogramBuffer_ = 0;
    GLuint sceneChangeBuffer_ = 0;
    GLuint sceneReadback_[3] = {0, 0, 0};
    int histogramSlot_ = 0;
    int readbackIndex_ = 0;
    float sceneMax_ = 0.f, sceneMean_ = 0.f, sceneMedian_ = 0.f;
    float sceneDifference_ = 0.f;
    bool sceneChanged_ = true;

    GLuint epePartials_ = 0;
    int epeCapacity_ = 0;

    Options options_;
    int displayWidth_ = 0;
    int displayHeight_ = 0;
};

}  // namespace opticalflow
