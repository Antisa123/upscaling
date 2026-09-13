#include "present/frame_pacer.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace present {

double FramePacer::nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

bool FramePacer::start(SDL_Window* window, SDL_GLContext presentContext, int width, int height,
                       bool vsync) {
    window_ = window;
    context_ = presentContext;
    width_ = width;
    height_ = height;
    vsync_ = vsync ? 1 : 0;
    free_.clear();
    for (int i = 0; i < kSlots; ++i) {
        const std::string base = "present.slot" + std::to_string(i);
        slots_[i].real.ensure(width, height, GL_RGBA8, 1, base + ".real");
        slots_[i].interpolated.ensure(width, height, GL_RGBA8, 1, base + ".interpolated");
        free_.push_back(i);
    }
    // The textures must exist on the GPU before the other context wraps them.
    glFinish();

    stopping_ = false;
    started_ = 0;
    thread_ = std::thread(&FramePacer::run, this);
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return started_ != 0; });
    if (started_ < 0) {
        lock.unlock();
        thread_.join();
        return false;
    }
    return true;
}

void FramePacer::stop() {
    if (!thread_.joinable()) return;
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    thread_.join();
}

void FramePacer::destroy() {
    for (Slot& s : slots_) {
        s.real.destroy();
        s.interpolated.destroy();
    }
}

int FramePacer::acquire() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return !free_.empty(); });
    const int index = free_.back();
    free_.pop_back();
    return index;
}

void FramePacer::submit(int index, long long frame, bool interpolated, double sampleMs) {
    std::unique_lock lock(mutex_);
    const long long id = ++submitted_;
    queue_.push_back(Item{id, index, frame, interpolated, sampleMs, nowMs()});
    cv_.notify_all();
    cv_.wait(lock, [&] { return staged_ >= id || started_ < 0; });
}

std::vector<PresentRecord> FramePacer::takeRecords() {
    std::lock_guard lock(statsMutex_);
    std::vector<PresentRecord> out;
    out.swap(records_);
    return out;
}

void FramePacer::recentIntervals(std::vector<float>& out) const {
    std::lock_guard lock(statsMutex_);
    out.clear();
    const int n = static_cast<int>(intervals_.size());
    out.reserve(n);
    // Oldest first: the ring is full once it has wrapped.
    for (int i = 0; i < n; ++i) out.push_back(intervals_[(intervalHead_ + i) % n]);
}

void FramePacer::run() {
    if (SDL_GL_MakeCurrent(window_, context_) != 0) {
        std::fprintf(stderr, "[pacer] present context unavailable: %s\n", SDL_GetError());
        {
            std::lock_guard lock(mutex_);
            started_ = -1;
        }
        cv_.notify_all();
        return;
    }
    int appliedVsync = vsync_;
    SDL_GL_SetSwapInterval(appliedVsync);

    // Framebuffer objects are containers and are not shared between contexts;
    // the textures they wrap are.
    GLuint framebuffers[kSlots][2] = {};
    for (int i = 0; i < kSlots; ++i) {
        glCreateFramebuffers(2, framebuffers[i]);
        glNamedFramebufferTexture(framebuffers[i][0], GL_COLOR_ATTACHMENT0, slots_[i].real.id, 0);
        glNamedFramebufferTexture(framebuffers[i][1], GL_COLOR_ATTACHMENT0,
                                  slots_[i].interpolated.id, 0);
    }
    {
        std::lock_guard lock(mutex_);
        started_ = 1;
    }
    cv_.notify_all();

    double lastReady = 0.0;
    for (;;) {
        Item item;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !queue_.empty() || stopping_; });
            if (queue_.empty()) break;
            item = queue_.front();
            queue_.pop_front();
        }
        const int wantedVsync = vsync_;
        if (wantedVsync != appliedVsync) {
            SDL_GL_SetSwapInterval(wantedVsync);
            appliedVsync = wantedVsync;
        }

        // The render period is measured where it is defined: between two pairs
        // becoming ready. A stall -- a shader reload, a window drag -- is not the
        // frame rate, and letting it into the average would hold the next
        // several real frames back by half of it.
        if (lastReady > 0.0) {
            const double period = item.readyMs - lastReady;
            if (period > 0.0 && period < 250.0) {
                const double p = periodMs_;
                periodMs_ = p > 0.0 ? p + 0.1 * (period - p) : period;
            }
        }
        lastReady = item.readyMs;

        double shown = 0.0;
        if (item.interpolated) {
            const double wait = stage(framebuffers[item.slot][1]);
            shown = swap(item, true, false, wait);
        }
        const double realWait = stage(framebuffers[item.slot][0]);
        {
            std::lock_guard lock(mutex_);
            staged_ = item.id;
        }
        cv_.notify_all();

        bool caughtUp = false;
        if (item.interpolated && pacing_ == PacingMode::Paced && periodMs_ > 0.0)
            caughtUp = waitUntil(shown + 0.5 * periodMs_);
        swap(item, false, caughtUp, realWait);

        {
            std::lock_guard lock(mutex_);
            free_.push_back(item.slot);
        }
        cv_.notify_all();
    }

    for (auto& pair : framebuffers) glDeleteFramebuffers(2, pair);
    SDL_GL_MakeCurrent(window_, nullptr);
}

bool FramePacer::waitUntil(double targetMs) {
    for (;;) {
        const double now = nowMs();
        if (now >= targetMs) return false;
        {
            // The next pair is already waiting: the presenter is behind, and
            // holding this frame back would only push the lag onto the next one.
            std::lock_guard lock(mutex_);
            if (!queue_.empty()) return true;
        }
        const double left = targetMs - now;
        // A sleep overshoots by up to a millisecond or so; sleep most of the
        // way and yield through the rest.
        if (left > 2.0)
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<long long>((left - 1.5) * 1000.0)));
        else
            std::this_thread::yield();
    }
}

double FramePacer::stage(GLuint framebuffer) {
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window_, &w, &h);
    const double submitted = nowMs();
    glBlitNamedFramebuffer(framebuffer, 0, 0, 0, width_, height_, 0, 0, w, h, GL_COLOR_BUFFER_BIT,
                           GL_LINEAR);
    // Finished here, so the swap has no GPU work left to wait for and the
    // recorded present time is when the image was complete. The wait is
    // recorded too: it is the part of the pacing error that comes from
    // sharing one GPU, and with staging it should be the blit alone.
    glFinish();
    return nowMs() - submitted;
}

double FramePacer::swap(const Item& item, bool interpolated, bool caughtUp, double gpuWaitMs) {
    SDL_GL_SwapWindow(window_);
    const double now = nowMs();

    PresentRecord r;
    r.frame = item.frame;
    r.interpolated = interpolated ? 1 : 0;
    r.sampleMs = item.sampleMs;
    r.readyMs = item.readyMs;
    r.presentMs = now;
    r.gpuWaitMs = gpuWaitMs;
    r.caughtUp = caughtUp ? 1 : 0;

    std::lock_guard lock(statsMutex_);
    records_.push_back(r);
    if (lastPresentMs_ > 0.0) {
        const float interval = static_cast<float>(now - lastPresentMs_);
        if (static_cast<int>(intervals_.size()) < kHistory) {
            intervals_.push_back(interval);
        } else {
            intervals_[intervalHead_] = interval;
            intervalHead_ = (intervalHead_ + 1) % kHistory;
        }
        const double fps = presentedFps_;
        const double instant = interval > 0.0f ? 1000.0 / interval : 0.0;
        presentedFps_ = fps > 0.0 ? fps + 0.05 * (instant - fps) : instant;
    }
    lastPresentMs_ = now;
    if (!interpolated) {
        const double l = latencyMs_;
        const double sample = now - item.sampleMs;
        latencyMs_ = l > 0.0 ? l + 0.05 * (sample - l) : sample;
    }
    return now;
}

}  // namespace present
