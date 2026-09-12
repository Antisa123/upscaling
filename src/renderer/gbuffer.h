#pragma once

#include "gfx/gpu_timer.h"
#include "gfx/shader.h"
#include "gfx/texture.h"
#include "renderer/camera.h"
#include "renderer/scene.h"

namespace renderer {

// Everything the upscaler and the frame generator consume is produced here:
// HDR colour, reverse-Z depth, UV-space motion vectors and a reactive mask.
class GBuffer {
public:
    void create(int renderWidth, int renderHeight);
    void resize(int renderWidth, int renderHeight);
    void destroy();

    // `useJitter` off renders with the unjittered projection, which is what a
    // ground-truth reference frame needs: no sub-pixel offset to undo.
    void render(const Scene& scene, const Camera& camera, gfx::GpuTimer& timer,
                bool useJitter = true, const char* scopeLabel = "G-buffer");

    // Texture LOD bias applied while sampling material textures. Negative
    // values pull in detail the render resolution would otherwise never sample;
    // see the comment in gbuffer.frag for why an upscaler needs it. The
    // reference render keeps it at 0 -- it is already at its own native
    // resolution and biasing it would move the target, not the candidate.
    void setMipBias(float bias) { mipBias_ = bias; }
    float mipBias() const { return mipBias_; }

    // Strength of the reactive mask written for alpha-tested surfaces; 0 turns
    // it off. See gbuffer.frag for why the alpha cutoff is the only honest
    // source of one in a scene without particles.
    void setReactiveScale(float scale) { reactiveScale_ = scale; }
    float reactiveScale() const { return reactiveScale_; }

    // Band-limit the procedural surface detail. Off by default: the aliasing
    // is the workload the upscaler has to resolve.
    void setPatternFilter(bool enabled) { patternFilter_ = enabled; }
    bool patternFilter() const { return patternFilter_; }
    void reloadShaders() { program_.reloadIfChanged(); }

    const gfx::Texture2D& color() const { return color_.current(); }
    // Last frame's colour at render resolution. Used by the reprojection
    // debug view now, and by every temporal pass later.
    const gfx::Texture2D& prevColor() const { return color_.history(); }
    const gfx::Texture2D& depth() const { return depth_; }
    const gfx::Texture2D& velocity() const { return velocity_; }
    const gfx::Texture2D& reactive() const { return reactive_; }
    // rg = (this frame's linear view distance, where this surface was last
    // frame). Ping-ponged, because the disocclusion test compares the second
    // channel against the *previous frame's* first channel.
    const gfx::Texture2D& depthInfo() const { return depthInfo_.current(); }
    const gfx::Texture2D& prevDepthInfo() const { return depthInfo_.history(); }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    gfx::PingPong color_;      // RGBA16F, linear HDR, current + previous
    gfx::Texture2D velocity_;  // RG16F, UV space, points current -> previous
    gfx::Texture2D reactive_;  // R8, 0 = trust history
    gfx::PingPong depthInfo_;  // RG32F, linear view distance: current + expected previous
    gfx::Texture2D depth_;     // DEPTH_COMPONENT32F, reverse-Z
    gfx::Framebuffer fbo_;
    gfx::Program program_;
    bool patternFilter_ = false;
    float mipBias_ = 0.f;
    float reactiveScale_ = 0.f;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace renderer
