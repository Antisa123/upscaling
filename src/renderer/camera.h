#pragma once

#include <glm/glm.hpp>

union SDL_Event;

namespace renderer {

// Free-fly camera with reverse-Z infinite projection and sub-pixel jitter.
//
// Three facts matter for everything downstream:
//  * reverse-Z (depth 1 at the near plane, 0 at infinity) gives far better
//    precision, and is the convention the FSR passes assume;
//  * the jittered projection is what the G-buffer renders with, but motion
//    vectors must be computed from the *unjittered* matrices, otherwise the
//    jitter pattern leaks into the velocity buffer and smears the history;
//  * previous-frame matrices are kept here so the renderer never has to.
class Camera {
public:
    void setPerspective(float fovYRadians, float nearPlane);
    void setResolution(int renderWidth, int renderHeight, int displayWidth, int displayHeight);

    // Advances the jitter phase and rolls current matrices into previous ones.
    // Call once per rendered frame, before building matrices.
    void beginFrame(bool jitterEnabled);

    void handleEvent(const SDL_Event& event);
    void update(float dt, const unsigned char* keyboardState);

    // Deterministic orbit used for reproducible benchmarks and ground-truth
    // captures. `t` is scene time in seconds.
    void applyScriptedPath(double t);
    // The scripted orbit is defined in scene units, so a scene loaded at a
    // different scale needs a different radius to frame the same thing.
    void setPathRadius(float radius) { pathRadius_ = radius; }
    float pathRadius() const { return pathRadius_; }
    // Starting angle of the orbit, in radians. Lets a scene be framed on
    // something worth measuring for the whole length of a capture.
    void setPathPhase(float radians) { pathPhase_ = radians; }
    void setScripted(bool on) { scripted_ = on; }
    bool scripted() const { return scripted_; }

    const glm::mat4& view() const { return view_; }
    const glm::mat4& prevView() const { return prevView_; }
    const glm::mat4& proj() const { return proj_; }              // unjittered
    const glm::mat4& projJittered() const { return projJittered_; }
    const glm::mat4& prevProj() const { return prevProj_; }      // unjittered
    glm::mat4 viewProj() const { return proj_ * view_; }
    glm::mat4 viewProjJittered() const { return projJittered_ * view_; }
    glm::mat4 prevViewProj() const { return prevProj_ * prevView_; }

    glm::vec3 position() const { return position_; }
    glm::vec2 jitter() const { return jitter_; }        // pixels, render resolution
    glm::vec2 prevJitter() const { return prevJitter_; }
    float nearPlane() const { return near_; }
    int jitterPhaseCount() const { return jitterPhases_; }
    // True when the camera moved discontinuously; upscaler history must reset.
    bool jumpCut() const { return jumpCut_; }
    void requestJumpCut() { jumpCut_ = true; }

    float moveSpeed = 3.0f;

private:
    void rebuildProjection();

    glm::vec3 position_{0.f, 1.6f, 4.f};
    float yaw_ = -90.f;   // degrees
    float pitch_ = 0.f;   // degrees
    float fovY_ = glm::radians(60.f);
    float near_ = 0.05f;

    int renderW_ = 1, renderH_ = 1, displayW_ = 1, displayH_ = 1;

    glm::mat4 view_{1.f}, prevView_{1.f};
    glm::mat4 proj_{1.f}, prevProj_{1.f}, projJittered_{1.f};
    glm::vec2 jitter_{0.f}, prevJitter_{0.f};

    int jitterIndex_ = 0;
    int jitterPhases_ = 8;
    bool scripted_ = false;
    float pathRadius_ = 5.f;
    float pathPhase_ = 0.f;
    bool jumpCut_ = true;  // first frame always resets history
    bool mouseCaptured_ = false;
};

// Halton radical inverse; the low-discrepancy sequence FSR uses for jitter.
float haltonSample(int index, int base);

}  // namespace renderer
