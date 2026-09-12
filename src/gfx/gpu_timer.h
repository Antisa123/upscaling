#pragma once

#include <string>
#include <vector>

#include "gfx/gl_common.h"

namespace gfx {

// Per-pass GPU timing built on GL_TIMESTAMP queries.
//
// This is the measurement backbone of the whole project: the thesis needs a
// cost breakdown per pass, not just a frame rate. Results are read back two
// frames late so the CPU never blocks on the GPU.
class GpuTimer {
public:
    static constexpr int kRing = 3;

    void begin(const std::string& name);
    void end();

    // Call once per frame, before any begin().
    void beginFrame();
    // Call once per frame, after the last end(); collects finished results.
    void endFrame();

    struct Result {
        std::string name;
        double ms = 0.0;       // smoothed
        double lastMs = 0.0;   // most recent sample
        int depth = 0;         // nesting level, for indented display
    };
    const std::vector<Result>& results() const { return results_; }
    // Sum of the top-level passes, excluding reference-only work.
    // Smoothed: readable on screen, useless as a measurement series.
    double totalMs() const;
    // The same sum for the most recent harvested frame, unsmoothed. This is
    // what belongs in a CSV: an exponential average of a warm-up spike stays
    // visible for dozens of frames and quietly biases every statistic.
    double lastTotalMs() const;

    // Passes that exist only to produce a reference image (a native-resolution
    // render the upscaler is scored against, for instance) are still timed and
    // reported, but must not count towards the frame cost: the pipeline being
    // measured does not run them. Name such a scope with this prefix.
    static constexpr const char* kReferencePrefix = "ref: ";
    static bool isReferencePass(const std::string& name) {
        return name.rfind(kReferencePrefix, 0) == 0;
    }

    void destroy();

private:
    struct Zone {
        std::string name;
        GLuint query[2] = {0, 0};
        int depth = 0;
    };
    struct Frame {
        std::vector<Zone> zones;
        int used = 0;
        bool pending = false;
    };

    Frame frames_[kRing];
    int frameIndex_ = 0;
    int depth_ = 0;
    std::vector<int> openStack_;
    std::vector<Result> results_;
};

// RAII helper: gfx::GpuScope scope(timer, "Reproject & accumulate");
class GpuScope {
public:
    GpuScope(GpuTimer& timer, const std::string& name) : timer_(timer) { timer_.begin(name); }
    ~GpuScope() { timer_.end(); }

private:
    GpuTimer& timer_;
};

}  // namespace gfx
