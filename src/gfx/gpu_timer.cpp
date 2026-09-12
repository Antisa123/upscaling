#include "gfx/gpu_timer.h"

#include <algorithm>

namespace gfx {

void GpuTimer::beginFrame() {
    frameIndex_ = (frameIndex_ + 1) % kRing;
    Frame& f = frames_[frameIndex_];
    f.used = 0;
    depth_ = 0;
    openStack_.clear();
}

void GpuTimer::begin(const std::string& name) {
    Frame& f = frames_[frameIndex_];
    if (f.used == static_cast<int>(f.zones.size())) {
        Zone z;
        glGenQueries(2, z.query);
        f.zones.push_back(z);
    }
    Zone& z = f.zones[f.used];
    z.name = name;
    z.depth = depth_++;
    glQueryCounter(z.query[0], GL_TIMESTAMP);
    openStack_.push_back(f.used);
    ++f.used;
}

void GpuTimer::end() {
    if (openStack_.empty()) return;
    Frame& f = frames_[frameIndex_];
    Zone& z = f.zones[openStack_.back()];
    openStack_.pop_back();
    --depth_;
    glQueryCounter(z.query[1], GL_TIMESTAMP);
}

void GpuTimer::endFrame() {
    frames_[frameIndex_].pending = true;

    // Harvest the oldest frame in the ring; by now its queries are done.
    Frame& old = frames_[(frameIndex_ + 1) % kRing];
    if (!old.pending || old.used == 0) return;

    GLint available = GL_TRUE;
    glGetQueryObjectiv(old.zones[old.used - 1].query[1], GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) return;

    for (int i = 0; i < old.used; ++i) {
        const Zone& z = old.zones[i];
        GLuint64 t0 = 0, t1 = 0;
        glGetQueryObjectui64v(z.query[0], GL_QUERY_RESULT, &t0);
        glGetQueryObjectui64v(z.query[1], GL_QUERY_RESULT, &t1);
        const double ms = static_cast<double>(t1 - t0) * 1e-6;

        auto it = std::find_if(results_.begin(), results_.end(),
                               [&](const Result& r) { return r.name == z.name; });
        if (it == results_.end()) {
            results_.push_back({z.name, ms, ms, z.depth});
        } else {
            it->lastMs = ms;
            // Exponential moving average keeps the on-screen numbers readable
            // without hiding real changes.
            it->ms = it->ms * 0.9 + ms * 0.1;
            it->depth = z.depth;
        }
    }
    old.pending = false;
}

double GpuTimer::totalMs() const {
    double sum = 0.0;
    for (const Result& r : results_)
        if (r.depth == 0 && !isReferencePass(r.name)) sum += r.ms;
    return sum;
}

double GpuTimer::lastTotalMs() const {
    double sum = 0.0;
    for (const Result& r : results_)
        if (r.depth == 0 && !isReferencePass(r.name)) sum += r.lastMs;
    return sum;
}

void GpuTimer::destroy() {
    for (Frame& f : frames_) {
        for (Zone& z : f.zones) glDeleteQueries(2, z.query);
        f.zones.clear();
        f.used = 0;
    }
    results_.clear();
}

}  // namespace gfx
