#pragma once

#include <string>
#include <vector>

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"

namespace present {

// M8: the on-screen HUD.
//
// A fixed-size panel in the top-left corner: a grid of monospace text lines
// and a present-interval graph under them. Rasterised by ui_hud.comp into a
// display-resolution RGBA8 layer that the compose pass puts over the frame --
// or, in the baked mode, into the frame before frame generation sees it.
class Hud {
public:
    static constexpr int kColumns = 52;
    static constexpr int kRows = 7;
    static constexpr int kGraphSamples = 240;

    bool create();
    void destroy();
    void reloadShaders();

    // Lines past kRows and characters past kColumns are dropped.
    void setLines(const std::vector<std::string>& lines);
    // Present intervals in ms, oldest first; an empty list hides the graph.
    void setGraph(const std::vector<float>& intervalsMs, float targetMs);

    void render(int displayWidth, int displayHeight, gfx::GpuTimer& timer);

    const gfx::Texture2D& layer() const { return layer_; }
    // Panel rectangle in GL image coordinates (origin bottom-left).
    int rectX() const { return kMargin; }
    int rectY() const { return layer_.height - kMargin - height(); }
    int width() const { return 2 * kPadding + kColumns * cellW_; }
    int height() const { return 3 * kPadding + kRows * cellH_ + kGraphHeight; }

private:
    static constexpr int kMargin = 12;
    static constexpr int kPadding = 6;
    static constexpr int kGraphHeight = 48;

    gfx::Program program_;
    gfx::Texture2D font_;
    gfx::Texture2D text_;
    gfx::Texture2D graph_;
    gfx::Texture2D layer_;
    int cellW_ = 0;
    int cellH_ = 0;
    std::vector<unsigned char> textData_;
    int graphCount_ = 0;
    float graphTarget_ = 0.f;
};

}  // namespace present
