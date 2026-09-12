#include "upscale/upscaler.h"

#include <cstring>

namespace upscale {

const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::None: return "nearest";
        case Mode::Bilinear: return "bilinear";
        case Mode::Bicubic: return "bicubic";
        case Mode::Fsr1: return "fsr1";
        case Mode::Taau: return "taau";
        case Mode::TaauRcas: return "taau-rcas";
        case Mode::Fsr: return "fsr";
        case Mode::FsrRcas: return "fsr-rcas";
        default: return "?";
    }
}

Mode modeFromName(const std::string& name) {
    for (int i = 0; i < static_cast<int>(Mode::Count); ++i) {
        const Mode mode = static_cast<Mode>(i);
        if (name == modeName(mode)) return mode;
    }
    return Mode::Count;
}

void Upscaler::create() {
    tonemap_ = gfx::Program::compute("tonemap.comp");
    downsample_ = gfx::Program::compute("downsample.comp");
    resample_ = gfx::Program::compute("upscale_resample.comp");
    easu_ = gfx::Program::compute("fsr1_easu.comp");
    rcas_ = gfx::Program::compute("fsr1_rcas.comp");
    taau_ = gfx::Program::compute("taau.comp");
    dilate_ = gfx::Program::compute("fsr_dilate.comp");
    locks_ = gfx::Program::compute("fsr_locks.comp");
    accumulate_ = gfx::Program::compute("fsr_accumulate.comp");
}

void Upscaler::destroy() {
    tonemap_.destroy();
    downsample_.destroy();
    resample_.destroy();
    easu_.destroy();
    rcas_.destroy();
    taau_.destroy();
    dilate_.destroy();
    locks_.destroy();
    accumulate_.destroy();
    dilated_.destroy();
    newLocks_.destroy();
    lockStatus_.destroy();
    output_.destroy();
    intermediate_.destroy();
    history_.destroy();
    historyValid_ = false;
    last_ = &output_;
}

void Upscaler::reloadShaders() {
    tonemap_.reloadIfChanged();
    downsample_.reloadIfChanged();
    resample_.reloadIfChanged();
    easu_.reloadIfChanged();
    rcas_.reloadIfChanged();
    taau_.reloadIfChanged();
    dilate_.reloadIfChanged();
    locks_.reloadIfChanged();
    accumulate_.reloadIfChanged();
}

void Upscaler::tonemap(const gfx::Texture2D& src, gfx::Texture2D& dst, gfx::GpuTimer& timer,
                       const char* label) {
    dst.ensure(src.width, src.height, GL_RGBA16F, 1, label);
    gfx::GpuScope scope(timer, label);
    tonemap_.bind();
    src.bindTexture(0);
    tonemap_.set("uInput", 0);
    dst.bindImage(0, GL_WRITE_ONLY);
    tonemap_.set("uExposure", 1.0f);
    tonemap_.dispatch(dst.width, dst.height);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Upscaler::downsample(const gfx::Texture2D& src, gfx::Texture2D& dst, int factor,
                          gfx::GpuTimer& timer, const char* label) {
    const int w = src.width / factor;
    const int h = src.height / factor;
    dst.ensure(w, h, GL_RGBA16F, 1, label);
    gfx::GpuScope scope(timer, label);
    downsample_.bind();
    src.bindTexture(0);
    downsample_.set("uInput", 0);
    dst.bindImage(0, GL_WRITE_ONLY);
    downsample_.set("uFactor", factor);
    downsample_.dispatch(w, h);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Upscaler::runRcas(const gfx::Texture2D& src, gfx::Texture2D& dst, gfx::GpuTimer& timer) {
    gfx::GpuScope scope(timer, "FSR1 RCAS");
    rcas_.bind();
    src.bindTexture(0);
    rcas_.set("uInput", 0);
    dst.bindImage(0, GL_WRITE_ONLY);
    rcas_.set("uSharpness", sharpness_);
    rcas_.dispatch(dst.width, dst.height);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

const gfx::Texture2D& Upscaler::dispatchTaau(const gfx::Texture2D& input, int outWidth,
                                             int outHeight, const TemporalInputs& temporal,
                                             gfx::GpuTimer& timer) {
    const bool resized = history_.current().width != outWidth ||
                         history_.current().height != outHeight;
    history_.ensure(outWidth, outHeight, GL_RGBA16F, "taau history");
    bool reset = temporal.reset || resized || !historyValid_;
    if (reset) {
        // Uninitialised RGBA16F storage can hold anything, including NaN, and
        // a NaN weighted by zero is still NaN. Clearing is cheaper than
        // defending against it in the shader every frame.
        history_.tex[0].clear();
        history_.tex[1].clear();
    }

    // Write the new accumulation into the slot that is about to become
    // history; the one we read from is last frame's result.
    history_.swap();

    gfx::Texture2D& dst = history_.current();
    const gfx::Texture2D& src = history_.history();

    gfx::GpuScope scope(timer, "TAAU");
    taau_.bind();
    input.bindTexture(0);
    taau_.set("uInput", 0);
    src.bindTexture(1);
    taau_.set("uHistory", 1);
    temporal.velocity->bindTexture(2);
    taau_.set("uVelocity", 2);
    dst.bindImage(0, GL_WRITE_ONLY);
    taau_.set("uInputSize", static_cast<float>(input.width), static_cast<float>(input.height));
    taau_.set("uInvInputSize", 1.f / static_cast<float>(input.width),
              1.f / static_cast<float>(input.height));
    taau_.set("uOutputSize", static_cast<float>(outWidth), static_cast<float>(outHeight));
    taau_.set("uInvOutputSize", 1.f / static_cast<float>(outWidth),
              1.f / static_cast<float>(outHeight));
    taau_.set("uJitter", temporal.jitterX, temporal.jitterY);
    taau_.set("uMaxWeight", maxAccumFrames_);
    taau_.set("uClampGamma", clampGamma_);
    taau_.set("uMotionDecay", motionDecay_);
    taau_.set("uKernel", kernel_);
    taau_.set("uReset", reset ? 1 : 0);
    taau_.dispatch(outWidth, outHeight);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    historyValid_ = true;
    return dst;
}

void Upscaler::runDilate(const TemporalInputs& temporal, gfx::GpuTimer& timer) {
    const gfx::Texture2D& velocity = *temporal.velocity;
    dilated_.ensure(velocity.width, velocity.height, GL_RGBA16F, 1, "fsr dilated");

    gfx::GpuScope scope(timer, "FSR dilate");
    dilate_.bind();
    velocity.bindTexture(0);
    dilate_.set("uVelocity", 0);
    temporal.depthInfo->bindTexture(1);
    dilate_.set("uDepthInfo", 1);
    temporal.prevDepthInfo->bindTexture(2);
    dilate_.set("uPrevDepthInfo", 2);
    dilated_.bindImage(0, GL_WRITE_ONLY);
    dilate_.set("uInputSize", static_cast<float>(velocity.width),
                static_cast<float>(velocity.height));
    dilate_.set("uInvInputSize", 1.f / static_cast<float>(velocity.width),
                1.f / static_cast<float>(velocity.height));
    dilate_.set("uDepthTolerance", depthTolerance_);
    dilate_.set("uDilate", dilate_enabled_ ? 1 : 0);
    dilate_.dispatch(velocity.width, velocity.height);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Upscaler::runLocks(const gfx::Texture2D& input, gfx::GpuTimer& timer) {
    newLocks_.ensure(input.width, input.height, GL_R8, 1, "fsr locks");

    gfx::GpuScope scope(timer, "FSR locks");
    locks_.bind();
    input.bindTexture(0);
    locks_.set("uInput", 0);
    newLocks_.bindImage(0, GL_WRITE_ONLY);
    locks_.set("uInputSize", static_cast<float>(input.width), static_cast<float>(input.height));
    locks_.set("uLockContrast", lockContrast_);
    locks_.dispatch(input.width, input.height);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

const gfx::Texture2D& Upscaler::dispatchFsr(const gfx::Texture2D& input, int outWidth,
                                            int outHeight, const TemporalInputs& temporal,
                                            gfx::GpuTimer& timer) {
    runDilate(temporal, timer);
    runLocks(input, timer);

    const bool resized = history_.current().width != outWidth ||
                         history_.current().height != outHeight;
    history_.ensure(outWidth, outHeight, GL_RGBA16F, "fsr history");
    lockStatus_.ensure(outWidth, outHeight, GL_RG16F, "fsr lock status");
    const bool reset = temporal.reset || resized || !historyValid_;
    if (reset) {
        history_.tex[0].clear();
        history_.tex[1].clear();
        lockStatus_.tex[0].clear();
        lockStatus_.tex[1].clear();
    }
    history_.swap();
    lockStatus_.swap();

    gfx::Texture2D& dst = history_.current();
    const gfx::Texture2D& src = history_.history();

    gfx::GpuScope scope(timer, "FSR accumulate");
    accumulate_.bind();
    input.bindTexture(0);
    accumulate_.set("uInput", 0);
    src.bindTexture(1);
    accumulate_.set("uHistory", 1);
    dilated_.bindTexture(2);
    accumulate_.set("uDilated", 2);
    temporal.reactive->bindTexture(3);
    accumulate_.set("uReactive", 3);
    newLocks_.bindTexture(4);
    accumulate_.set("uNewLocks", 4);
    lockStatus_.history().bindTexture(5);
    accumulate_.set("uLockHistory", 5);
    dst.bindImage(0, GL_WRITE_ONLY);
    lockStatus_.current().bindImage(1, GL_WRITE_ONLY);
    accumulate_.set("uInputSize", static_cast<float>(input.width),
                    static_cast<float>(input.height));
    accumulate_.set("uInvInputSize", 1.f / static_cast<float>(input.width),
                    1.f / static_cast<float>(input.height));
    accumulate_.set("uOutputSize", static_cast<float>(outWidth), static_cast<float>(outHeight));
    accumulate_.set("uInvOutputSize", 1.f / static_cast<float>(outWidth),
                    1.f / static_cast<float>(outHeight));
    accumulate_.set("uJitter", temporal.jitterX, temporal.jitterY);
    accumulate_.set("uMaxWeight", maxAccumFrames_);
    accumulate_.set("uClampGamma", clampGamma_);
    accumulate_.set("uMotionDecay", motionDecay_);
    accumulate_.set("uLanczosScale", lanczosScale_);
    accumulate_.set("uLanczosScaleStill", lanczosScaleStill_);
    accumulate_.set("uDisocclusionStrength", disocclusionStrength_);
    accumulate_.set("uLockLife", lockLife_);
    accumulate_.set("uLockRelax", lockRelax_);
    accumulate_.set("uLockTolerance", lockTolerance_);
    accumulate_.set("uReset", reset ? 1 : 0);
    accumulate_.dispatch(outWidth, outHeight);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    historyValid_ = true;
    return dst;
}

const gfx::Texture2D& Upscaler::dispatch(const gfx::Texture2D& input, int outWidth, int outHeight,
                                         Mode mode, gfx::GpuTimer& timer,
                                         const TemporalInputs& temporal) {
    if (isTemporal(mode)) {
        const gfx::Texture2D& accumulated =
            isFullUpscaler(mode) ? dispatchFsr(input, outWidth, outHeight, temporal, timer)
                                 : dispatchTaau(input, outWidth, outHeight, temporal, timer);
        if (mode == Mode::Taau || mode == Mode::Fsr) {
            last_ = &accumulated;
            return accumulated;
        }
        output_.ensure(outWidth, outHeight, GL_RGBA16F, 1, "upscale output");
        runRcas(accumulated, output_, timer);
        last_ = &output_;
        return output_;
    }

    output_.ensure(outWidth, outHeight, GL_RGBA16F, 1, "upscale output");
    last_ = &output_;

    const float invIn[2] = {1.f / static_cast<float>(input.width),
                            1.f / static_cast<float>(input.height)};

    if (mode == Mode::Fsr1) {
        intermediate_.ensure(outWidth, outHeight, GL_RGBA16F, 1, "easu output");
        {
            gfx::GpuScope scope(timer, "FSR1 EASU");
            easu_.bind();
            input.bindTexture(0);
            easu_.set("uInput", 0);
            intermediate_.bindImage(0, GL_WRITE_ONLY);
            easu_.set("uInputSize", static_cast<float>(input.width),
                      static_cast<float>(input.height));
            easu_.set("uInvInputSize", invIn[0], invIn[1]);
            easu_.set("uOutputSize", static_cast<float>(outWidth), static_cast<float>(outHeight));
            easu_.dispatch(outWidth, outHeight);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
        runRcas(intermediate_, output_, timer);
        return output_;
    }

    gfx::GpuScope scope(timer, mode == Mode::Bicubic ? "Bicubic" :
                               mode == Mode::Bilinear ? "Bilinear" : "Nearest");
    resample_.bind();
    input.bindTexture(0);
    resample_.set("uInput", 0);
    output_.bindImage(0, GL_WRITE_ONLY);
    resample_.set("uMode", static_cast<int>(mode));
    resample_.set("uInputSize", static_cast<float>(input.width),
                  static_cast<float>(input.height));
    resample_.set("uInvInputSize", invIn[0], invIn[1]);
    resample_.set("uOutputSize", static_cast<float>(outWidth), static_cast<float>(outHeight));
    resample_.dispatch(outWidth, outHeight);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    return output_;
}

}  // namespace upscale
