#include "renderer/camera.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace renderer {

float haltonSample(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

void Camera::setPerspective(float fovYRadians, float nearPlane) {
    fovY_ = fovYRadians;
    near_ = nearPlane;
    rebuildProjection();
}

void Camera::setResolution(int renderWidth, int renderHeight, int displayWidth, int displayHeight) {
    renderW_ = std::max(1, renderWidth);
    renderH_ = std::max(1, renderHeight);
    displayW_ = std::max(1, displayWidth);
    displayH_ = std::max(1, displayHeight);

    // FSR's recommendation: phase count scales with the square of the upscale
    // ratio, so every display pixel eventually receives a sample.
    const float ratio = static_cast<float>(displayW_) / static_cast<float>(renderW_);
    jitterPhases_ = std::max(1, static_cast<int>(std::ceil(8.0f * ratio * ratio)));
    rebuildProjection();
}

void Camera::rebuildProjection() {
    const float aspect = static_cast<float>(renderW_) / static_cast<float>(renderH_);
    const float f = 1.0f / std::tan(fovY_ * 0.5f);

    // Reverse-Z with an infinite far plane, for a [0,1] clip range
    // (glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)).
    proj_ = glm::mat4(0.f);
    proj_[0][0] = f / aspect;
    proj_[1][1] = f;
    proj_[2][3] = -1.f;
    proj_[3][2] = near_;

    projJittered_ = proj_;
    // clip.x = P00*e.x + P20*e.z, w = -e.z  =>  ndc.x shift is -P20.
    projJittered_[2][0] = -2.0f * jitter_.x / static_cast<float>(renderW_);
    projJittered_[2][1] = -2.0f * jitter_.y / static_cast<float>(renderH_);
}

void Camera::beginFrame(bool jitterEnabled) {
    prevView_ = view_;
    prevProj_ = proj_;
    prevJitter_ = jitter_;

    if (jitterEnabled) {
        // Halton(2,3) in [-0.5, 0.5] pixels of render resolution.
        jitterIndex_ = (jitterIndex_ + 1) % jitterPhases_;
        jitter_.x = haltonSample(jitterIndex_ + 1, 2) - 0.5f;
        jitter_.y = haltonSample(jitterIndex_ + 1, 3) - 0.5f;
    } else {
        jitter_ = glm::vec2(0.f);
    }
    rebuildProjection();
}

void Camera::handleEvent(const SDL_Event& event) {
    if (scripted_) return;

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        mouseCaptured_ = true;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
        mouseCaptured_ = false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
    } else if (event.type == SDL_MOUSEMOTION && mouseCaptured_) {
        const float sensitivity = 0.12f;
        yaw_ += static_cast<float>(event.motion.xrel) * sensitivity;
        pitch_ -= static_cast<float>(event.motion.yrel) * sensitivity;
        pitch_ = std::clamp(pitch_, -89.f, 89.f);
    } else if (event.type == SDL_MOUSEWHEEL) {
        moveSpeed = std::clamp(moveSpeed * (event.wheel.y > 0 ? 1.25f : 0.8f), 0.05f, 100.f);
    }
}

void Camera::update(float dt, const unsigned char* keys) {
    jumpCut_ = false;

    const glm::vec3 forward = glm::normalize(glm::vec3(
        std::cos(glm::radians(yaw_)) * std::cos(glm::radians(pitch_)),
        std::sin(glm::radians(pitch_)),
        std::sin(glm::radians(yaw_)) * std::cos(glm::radians(pitch_))));
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));

    if (!scripted_ && keys) {
        float speed = moveSpeed * dt;
        if (keys[SDL_SCANCODE_LSHIFT]) speed *= 4.f;
        if (keys[SDL_SCANCODE_W]) position_ += forward * speed;
        if (keys[SDL_SCANCODE_S]) position_ -= forward * speed;
        if (keys[SDL_SCANCODE_A]) position_ -= right * speed;
        if (keys[SDL_SCANCODE_D]) position_ += right * speed;
        if (keys[SDL_SCANCODE_E]) position_ += glm::vec3(0.f, 1.f, 0.f) * speed;
        if (keys[SDL_SCANCODE_Q]) position_ -= glm::vec3(0.f, 1.f, 0.f) * speed;
    }

    view_ = glm::lookAt(position_, position_ + forward, glm::vec3(0.f, 1.f, 0.f));
}

void Camera::applyScriptedPath(double t) {
    // Clearing the cut flag here and in update() is what keeps it a one-frame
    // signal; a temporal pass that saw it stuck on would throw its history
    // away every frame and silently degrade to a spatial filter.
    jumpCut_ = false;

    // Orbit with a slow vertical bob plus a yaw sweep. Deterministic in `t`, so
    // a capture run reproduces exactly the same frames every time - which is
    // what makes PSNR/SSIM numbers comparable between configurations.
    const float radius = pathRadius_;
    const float angle = pathPhase_ + static_cast<float>(t) * 0.35f;
    // The bob and the look-at height scale with the radius so the same path
    // frames the same thing on a scene loaded at a different scale.
    const float h = radius * 0.2f;
    position_ = glm::vec3(std::cos(angle) * radius,
                          1.6f * h / 1.0f + 0.4f * h * std::sin(static_cast<float>(t) * 0.5f),
                          std::sin(angle) * radius);

    const glm::vec3 target(0.f, h, 0.f);
    const glm::vec3 dir = glm::normalize(target - position_);
    pitch_ = glm::degrees(std::asin(dir.y));
    yaw_ = glm::degrees(std::atan2(dir.z, dir.x));
    view_ = glm::lookAt(position_, target, glm::vec3(0.f, 1.f, 0.f));
}

}  // namespace renderer
