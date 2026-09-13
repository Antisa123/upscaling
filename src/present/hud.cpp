#include "present/hud.h"

#include <stb_image.h>

#include <algorithm>
#include <cstdio>

namespace present {

bool Hud::create() {
    // scripts/make_font_atlas.py: 16 x 6 cells covering ASCII 32..127.
    const std::string path = std::string(FSR3LITE_ASSET_DIR) + "/ui_font.png";
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        std::fprintf(stderr, "[hud] cannot load %s\n", path.c_str());
        return false;
    }
    cellW_ = w / 16;
    cellH_ = h / 6;
    font_.create(w, h, GL_RGBA8, 1, "hud.font");
    glTextureSubImage2D(font_.id, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    text_.create(kColumns, kRows, GL_R8UI, 1, "hud.text");
    glTextureParameteri(text_.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(text_.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    graph_.create(kGraphSamples, 1, GL_R32F, 1, "hud.graph");
    textData_.assign(static_cast<size_t>(kColumns) * kRows, ' ');

    program_ = gfx::Program::compute("ui_hud.comp");
    return program_.valid();
}

void Hud::destroy() {
    program_.destroy();
    font_.destroy();
    text_.destroy();
    graph_.destroy();
    layer_.destroy();
}

void Hud::reloadShaders() { program_.reloadIfChanged(); }

void Hud::setLines(const std::vector<std::string>& lines) {
    std::fill(textData_.begin(), textData_.end(), ' ');
    for (int row = 0; row < kRows && row < static_cast<int>(lines.size()); ++row) {
        const std::string& line = lines[row];
        const int n = std::min(static_cast<int>(line.size()), kColumns);
        for (int col = 0; col < n; ++col)
            textData_[static_cast<size_t>(row) * kColumns + col] = static_cast<unsigned char>(line[col]);
    }
    glTextureSubImage2D(text_.id, 0, 0, 0, kColumns, kRows, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                        textData_.data());
}

void Hud::setGraph(const std::vector<float>& intervalsMs, float targetMs) {
    graphCount_ = std::min(static_cast<int>(intervalsMs.size()), kGraphSamples);
    graphTarget_ = targetMs;
    if (graphCount_ == 0) return;
    // The newest samples, oldest first.
    const float* first = intervalsMs.data() + (intervalsMs.size() - graphCount_);
    glTextureSubImage2D(graph_.id, 0, 0, 0, graphCount_, 1, GL_RED, GL_FLOAT, first);
}

void Hud::render(int displayWidth, int displayHeight, gfx::GpuTimer& timer) {
    if (layer_.width != displayWidth || layer_.height != displayHeight) {
        layer_.ensure(displayWidth, displayHeight, GL_RGBA8, 1, "hud.layer");
        // The panel rewrites every one of its own pixels each frame; the rest of
        // the layer only ever needs to be clear once.
        layer_.clear();
    }

    gfx::GpuScope scope(timer, "HUD");
    program_.bind();
    font_.bindTexture(0);
    program_.set("uFont", 0);
    text_.bindTexture(1);
    program_.set("uText", 1);
    graph_.bindTexture(2);
    program_.set("uGraph", 2);
    layer_.bindImage(0, GL_WRITE_ONLY);
    program_.set("uDisplaySize", displayWidth, displayHeight);
    glUniform4i(glGetUniformLocation(program_.id(), "uRect"), kMargin, kMargin, width(), height());
    program_.set("uCell", cellW_, cellH_);
    program_.set("uTextGrid", kColumns, kRows);
    program_.set("uPadding", kPadding);
    program_.set("uGraphCount", graphCount_);
    program_.set("uGraphHeight", kGraphHeight);
    // Two target intervals at the top: a hitch of double length still fits.
    const float target = graphTarget_ > 0.f ? graphTarget_ : 16.667f;
    program_.set("uGraphScale", 2.5f * target);
    program_.set("uGraphTarget", target);
    program_.dispatch(width(), height());
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

}  // namespace present
