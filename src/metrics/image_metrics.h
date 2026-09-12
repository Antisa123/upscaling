#pragma once

#include <string>
#include <vector>

#include "gfx/shader.h"
#include "gfx/texture.h"

namespace metrics {

struct CompareResult {
    double mse = 0.0;   // mean squared error, per colour channel, [0,1] range
    double mae = 0.0;   // mean absolute error
    double psnr = 0.0;  // dB, computed from mse with peak 1.0
    double maxError = 0.0;
    double coverage = 1.0;  // fraction of pixels that contributed
    double ssim = 0.0;      // mean structural similarity, [-1,1], 1 = identical
    double minSsim = 0.0;   // worst window, exposes localised artefacts
};

// GPU image comparison used both for validating motion vectors and, later, for
// scoring upscaled and interpolated frames against ground truth.
//
// The reduction is two-stage: a compute shader reduces each workgroup tile into
// one partial sum, and the (few thousand) partials are summed on the CPU. That
// avoids the precision loss of a single atomic accumulator and the overflow
// traps of fixed-point atomics.
class ImageMetrics {
public:
    void create();
    void destroy();

    enum class Mode {
        Direct,     // compare a(uv) with b(uv)
        Reprojected // compare a(uv) with b(uv + velocity(uv))
    };

    // `velocity` may be null for Mode::Direct. When `tonemap` is set, both
    // images are Reinhard-tonemapped and gamma encoded first, which is the
    // domain PSNR/SSIM are conventionally reported in.
    CompareResult compare(const gfx::Texture2D& a, const gfx::Texture2D& b, Mode mode,
                          const gfx::Texture2D* velocity = nullptr, bool tonemap = true);

    // SSIM is an extra three passes, so it is opt-in: the per-frame validation
    // loop only needs PSNR, the thesis result tables need both.
    void setComputeSsim(bool enabled) { computeSsim_ = enabled; }

    void reloadShaders();

private:
    static constexpr int kTile = 16;

    void computeSsim(const gfx::Texture2D& a, const gfx::Texture2D& b, Mode mode,
                     const gfx::Texture2D* velocity, bool tonemap, CompareResult& result);
    void ensurePartials(int groupCount);

    gfx::Program reduce_;
    gfx::Program ssimPrepare_;
    gfx::Program gaussian_;
    gfx::Program ssimReduce_;

    GLuint partials_ = 0;
    int partialCapacity_ = 0;

    // Ping-pong pair for the separable Gaussian over the SSIM moment images.
    gfx::Texture2D stats0_[2];
    gfx::Texture2D stats1_[2];

    bool computeSsim_ = false;
};

}  // namespace metrics
