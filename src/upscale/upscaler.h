#pragma once

#include <string>

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"

namespace upscale {

// Everything that can sit between the render-resolution G-buffer and the
// screen. The first four are single-frame: they see one colour buffer and
// nothing else, which is exactly the point -- the gap between their numbers and
// the temporal ones is the value temporal accumulation adds.
enum class Mode {
    None,      // nearest, i.e. what the render target looks like blown up
    Bilinear,  // hardware bilinear, the trivial baseline
    Bicubic,   // Catmull-Rom, the usual "good" non-adaptive resampler
    Fsr1,      // EASU (edge-adaptive) + RCAS (contrast-adaptive sharpening)
    Taau,      // M4: reprojection + accumulation + neighbourhood clamp
    TaauRcas,  // the same, with FSR1's sharpening pass on the result
    Fsr,       // M5: dilated vectors, depth disocclusion, Lanczos, reactive
    FsrRcas,   // the same, with sharpening
    Count
};

const char* modeName(Mode mode);
// Returns Mode::Count if the name matches nothing.
Mode modeFromName(const std::string& name);

// True for the modes that consume a velocity buffer and a jitter offset.
// Those modes also need the camera to *keep* jittering: a spatial filter has
// nowhere to put the sub-pixel offsets, a temporal one lives off them.
inline bool isTemporal(Mode mode) {
    return mode == Mode::Taau || mode == Mode::TaauRcas || mode == Mode::Fsr ||
           mode == Mode::FsrRcas;
}

// True for the M5 modes, which need the depth pair on top of the velocity
// buffer because their disocclusion test is evidence-based rather than a guess
// made from colour.
inline bool isFullUpscaler(Mode mode) { return mode == Mode::Fsr || mode == Mode::FsrRcas; }

// Per-frame inputs the temporal modes need on top of the colour buffer.
struct TemporalInputs {
    const gfx::Texture2D* velocity = nullptr;  // render resolution, UV space
    // (this frame's linear view distance, the distance the same surface had
    // last frame) and the previous frame's copy of the first channel. Only the
    // M5 modes read them.
    const gfx::Texture2D* depthInfo = nullptr;
    const gfx::Texture2D* prevDepthInfo = nullptr;
    const gfx::Texture2D* reactive = nullptr;  // render resolution, 0 = trust history
    float jitterX = 0.f;                       // render pixels
    float jitterY = 0.f;
    // Set on the first frame, after a resize, and on a camera cut: there is no
    // history worth reprojecting.
    bool reset = false;
};

// Runs one of the upscalers into a display-resolution target.
//
// Everything here is compute and writes through an image unit, so the output
// can be fed straight into the metrics passes or the present pass without a
// framebuffer round trip.
class Upscaler {
public:
    void create();
    void destroy();
    void reloadShaders();

    // Linear HDR -> display-referred [0,1], the domain every pass below (and
    // every metric) is defined in. `dst` is sized to match `src`.
    void tonemap(const gfx::Texture2D& src, gfx::Texture2D& dst, gfx::GpuTimer& timer,
                 const char* label = "Tonemap");

    // Integer box downsample, used to turn a supersampled reference render
    // into an antialiased one. Averaging happens in whatever space `src` is
    // in, so call this *before* tone mapping.
    void downsample(const gfx::Texture2D& src, gfx::Texture2D& dst, int factor,
                    gfx::GpuTimer& timer, const char* label = "Downsample");

    // `input` is render resolution and already tone mapped; the output is
    // `outWidth` x `outHeight` in the same space. Returns the produced
    // texture, which is also what output() gives until the next call.
    const gfx::Texture2D& dispatch(const gfx::Texture2D& input, int outWidth, int outHeight,
                                   Mode mode, gfx::GpuTimer& timer,
                                   const TemporalInputs& temporal = {});

    const gfx::Texture2D& output() const { return *last_; }
    // Render-resolution dilate output, for the disocclusion debug view. Empty
    // until an M5 mode has run at least once.
    const gfx::Texture2D& dilated() const { return dilated_; }

    // RCAS sharpening strength in stops of attenuation: 0 is the strongest
    // useful setting and ~8 turns the pass off.
    void setSharpness(float sharpness) { sharpness_ = sharpness; }
    float sharpness() const { return sharpness_; }

    // Accumulation cap for the temporal modes, in frames.
    void setMaxAccumFrames(float frames) { maxAccumFrames_ = frames; }
    float maxAccumFrames() const { return maxAccumFrames_; }

    // Width of the temporal clamp box, in neighbourhood standard deviations.
    void setClampGamma(float gamma) { clampGamma_ = gamma; }
    float clampGamma() const { return clampGamma_; }

    // Decay of the accumulation cap with image-space motion, per display pixel.
    void setMotionDecay(float decay) { motionDecay_ = decay; }
    float motionDecay() const { return motionDecay_; }

    // Inverse variance of the Gaussian reconstruction kernel used by the M4
    // path, in render pixels^-2.
    void setKernel(float k) { kernel_ = k; }
    float kernel() const { return kernel_; }

    // Radial scale of the M5 path's Lanczos-2 kernel, while the image is
    // moving and with a still camera. 1 is the textbook support of two render
    // pixels; larger narrows it.
    void setLanczosScale(float moving, float still) {
        lanczosScale_ = moving;
        lanczosScaleStill_ = still;
    }
    float lanczosScale() const { return lanczosScale_; }
    float lanczosScaleStill() const { return lanczosScaleStill_; }

    // Depth mismatch tolerated before a pixel counts as disoccluded, relative
    // to its distance from the camera.
    void setDilate(bool enabled) { dilate_enabled_ = enabled; }
    bool dilateEnabled() const { return dilate_enabled_; }

    void setDepthTolerance(float t) { depthTolerance_ = t; }
    float depthTolerance() const { return depthTolerance_; }

    // 0 turns the depth-based history rejection off, leaving only the colour
    // clamp -- the M4 behaviour, and the ablation row M5 has to beat.
    void setDisocclusionStrength(float s) { disocclusionStrength_ = s; }
    float disocclusionStrength() const { return disocclusionStrength_; }

    // Lock lifetime in frames (0 disables locking), how far a lock widens the
    // clamp box, the luminance change that invalidates one, and the luminance
    // contrast a thin feature needs before it is locked at all.
    void setLocks(float life, float relax, float tolerance, float contrast) {
        lockLife_ = life;
        lockRelax_ = relax;
        lockTolerance_ = tolerance;
        lockContrast_ = contrast;
    }
    float lockLife() const { return lockLife_; }

    // Forces the next temporal dispatch to start from scratch.
    void resetHistory() { historyValid_ = false; }

private:
    const gfx::Texture2D& dispatchTaau(const gfx::Texture2D& input, int outWidth, int outHeight,
                                       const TemporalInputs& temporal, gfx::GpuTimer& timer);
    const gfx::Texture2D& dispatchFsr(const gfx::Texture2D& input, int outWidth, int outHeight,
                                      const TemporalInputs& temporal, gfx::GpuTimer& timer);
    // Render-resolution pass shared by the M5 modes: dilates the motion vectors
    // towards the nearest surface and turns the depth pair into a disocclusion
    // factor.
    void runDilate(const TemporalInputs& temporal, gfx::GpuTimer& timer);
    // Render-resolution thin-feature detector feeding the lock state machine.
    void runLocks(const gfx::Texture2D& input, gfx::GpuTimer& timer);
    void runRcas(const gfx::Texture2D& src, gfx::Texture2D& dst, gfx::GpuTimer& timer);

    gfx::Program tonemap_;
    gfx::Program downsample_;
    gfx::Program resample_;  // none / bilinear / bicubic, selected by uniform
    gfx::Program easu_;
    gfx::Program rcas_;
    gfx::Program taau_;
    gfx::Program dilate_;
    gfx::Program locks_;
    gfx::Program accumulate_;

    gfx::Texture2D output_;
    // EASU writes here and RCAS reads it; separate because RCAS is a
    // neighbourhood filter and cannot run in place.
    gfx::Texture2D intermediate_;
    // Display-resolution accumulation buffer: rgb is the upscaled image, alpha
    // is how many frames' worth of samples are in it.
    gfx::PingPong history_;
    // Render resolution: xy = dilated motion vector, z = disocclusion, w = the
    // nearest depth in the 3x3 window.
    gfx::Texture2D dilated_;
    // Render resolution, 1 where a thin feature was detected this frame.
    gfx::Texture2D newLocks_;
    // Display resolution: x = frames of lock left, y = the luminance the lock
    // was created at. Ping-ponged with the colour history it travels with.
    gfx::PingPong lockStatus_;
    bool historyValid_ = false;

    // Whatever the last dispatch produced. The temporal modes return a
    // ping-pong slot rather than output_, so a plain member would have to be
    // copied every frame for nothing.
    const gfx::Texture2D* last_ = &output_;

    float sharpness_ = 1.2f;
    float maxAccumFrames_ = 8.f;
    float clampGamma_ = 2.f;
    float motionDecay_ = 1.6f;
    float kernel_ = 6.f;
    float lanczosScale_ = 1.f;
    float lanczosScaleStill_ = 2.f;
    bool dilate_enabled_ = true;
    float depthTolerance_ = 0.02f;
    float disocclusionStrength_ = 1.f;
    float lockLife_ = 4.f;
    float lockRelax_ = 2.f;
    float lockTolerance_ = 0.1f;
    float lockContrast_ = 0.5f;
};

}  // namespace upscale
