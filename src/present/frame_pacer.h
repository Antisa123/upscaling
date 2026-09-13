#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "gfx/texture.h"

namespace present {

enum class PacingMode {
    // Generated and real frame back to back, as soon as the pair is ready. The
    // frame counter doubles; what the eye sees is two images a fraction of a
    // millisecond apart followed by a whole frame of nothing.
    Immediate,
    // The generated frame as soon as the pair is ready, the real one half a
    // render period later. This is the only order that turns twice the frames
    // into twice the smoothness.
    Paced,
};

// One presented image, as the presenter saw it.
struct PresentRecord {
    long long frame = 0;     // render frame the image belongs to
    int interpolated = 0;    // 1 = the generated frame between frame-1 and frame
    double sampleMs = 0.0;   // when the render thread sampled input for `frame`
    double readyMs = 0.0;    // when the pair was handed to the presenter
    double presentMs = 0.0;  // when the swap returned
    double gpuWaitMs = 0.0;  // blit submitted -> finished; time spent behind the render thread's GPU work
    int caughtUp = 0;        // 1 = pacing wait cut short because the next pair was already waiting
};

// Two display-resolution images handed over together: the real frame and,
// when frame generation is presenting, the generated one before it.
struct Slot {
    gfx::Texture2D real;
    gfx::Texture2D interpolated;
};

// M8: frame pacing.
//
// A dedicated presenter thread with its own GL context, sharing textures with
// the render context. It exists because the two things a paced frame pair
// needs cannot happen on one thread: the real frame has to reach the screen
// half a render period after the generated one, and during that half period
// the render thread must already be working on the next frame. Waiting on the
// render thread instead makes the frame period the render time plus the wait,
// which gives the frame rate back.
//
// The render thread fills a slot, finishes its GPU work and submits; the
// presenter shows the generated image, waits, shows the real one and returns
// the slot. With every slot in flight acquire() blocks, which is the same
// back-pressure a swap chain applies.
//
// Both images are put into the window's back buffer while the GPU is idle,
// and submit() does not return until they are: the generated one is on screen
// and the real one is drawn and finished, waiting only for its swap. Two
// contexts on one GPU do not get to cut in line -- the kernel runs jobs in
// the order they were submitted -- so a blit issued half a frame later sits
// behind whatever the render thread has queued by then, for a delay as long
// and as variable as the render work itself. Measured, that was 3-6 ms on a
// 13 ms frame, and it put the "paced" frames right back next to each other.
// Staging first leaves only the swap on the clock.
//
// Handoff uses glFinish rather than a fence: on the driver this was built on
// (Mesa 26.2, radeonsi) glFenceSync(GL_SYNC_FENCE, 0) fails with
// GL_INVALID_ENUM. The render thread only has a few hundred microseconds of
// CPU work per frame, so blocking it on the GPU costs almost nothing, and it
// makes "ready" a measured time instead of a queued one.
class FramePacer {
public:
    static constexpr int kSlots = 3;
    static constexpr int kHistory = 240;

    // Render thread, render context current. `presentContext` must share
    // objects with it and not be current on any thread.
    bool start(SDL_Window* window, SDL_GLContext presentContext, int width, int height, bool vsync);
    // Presents whatever is still queued, then joins the thread.
    void stop();
    // Render thread, after stop().
    void destroy();

    // Blocks until a slot is free. The slot's textures belong to the caller
    // until submit().
    int acquire();
    Slot& slot(int index) { return slots_[index]; }
    // The GPU work that filled the slot must be finished when this is called.
    // Returns once the presenter has staged the slot (see above).
    void submit(int index, long long frame, bool interpolated, double sampleMs);

    void setPacing(PacingMode mode) { pacing_ = mode; }
    PacingMode pacing() const { return pacing_; }
    void setVsync(bool enabled) { vsync_ = enabled ? 1 : 0; }

    static double nowMs();

    // Everything presented since the last call.
    std::vector<PresentRecord> takeRecords();
    // For the HUD: smoothed rates and the most recent present intervals.
    double presentedFps() const { return presentedFps_; }
    double renderFps() const { return periodMs_ > 0.0 ? 1000.0 / periodMs_ : 0.0; }
    double latencyMs() const { return latencyMs_; }
    void recentIntervals(std::vector<float>& out) const;

private:
    struct Item {
        long long id = 0;
        int slot = 0;
        long long frame = 0;
        bool interpolated = false;
        double sampleMs = 0.0;
        double readyMs = 0.0;
    };

    void run();
    // Draws a slot image into the back buffer and waits for the GPU; returns
    // how long that took.
    double stage(GLuint framebuffer);
    double swap(const Item& item, bool interpolated, bool caughtUp, double gpuWaitMs);
    bool waitUntil(double targetMs);

    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    Slot slots_[kSlots];

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Item> queue_;
    std::vector<int> free_;
    bool stopping_ = false;
    long long submitted_ = 0;
    long long staged_ = 0;
    int started_ = 0;  // 0 pending, 1 running, -1 failed

    std::atomic<PacingMode> pacing_{PacingMode::Paced};
    std::atomic<int> vsync_{0};
    std::atomic<double> periodMs_{0.0};
    std::atomic<double> presentedFps_{0.0};
    std::atomic<double> latencyMs_{0.0};

    mutable std::mutex statsMutex_;
    std::vector<PresentRecord> records_;
    std::vector<float> intervals_;
    int intervalHead_ = 0;
    double lastPresentMs_ = 0.0;
};

}  // namespace present
