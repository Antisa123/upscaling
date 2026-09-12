#include "renderer/gbuffer.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace renderer {

void GBuffer::create(int renderWidth, int renderHeight) {
    program_ = gfx::Program::graphics("gbuffer.vert", "gbuffer.frag");
    fbo_.create("gbuffer");
    resize(renderWidth, renderHeight);
}

void GBuffer::resize(int renderWidth, int renderHeight) {
    if (width_ == renderWidth && height_ == renderHeight) return;
    width_ = renderWidth;
    height_ = renderHeight;

    color_.ensure(width_, height_, GL_RGBA16F, "gbuffer.color");
    velocity_.ensure(width_, height_, GL_RG16F, 1, "gbuffer.velocity");
    reactive_.ensure(width_, height_, GL_R8, 1, "gbuffer.reactive");
    depthInfo_.ensure(width_, height_, GL_RG32F, "gbuffer.depthInfo");
    depth_.ensure(width_, height_, GL_DEPTH_COMPONENT32F, 1, "gbuffer.depth");

    fbo_.attachColor(0, color_.current());
    fbo_.attachColor(1, velocity_);
    fbo_.attachColor(2, reactive_);
    fbo_.attachColor(3, depthInfo_.current());
    fbo_.attachDepth(depth_);
    fbo_.finalizeAttachments();
}

void GBuffer::render(const Scene& scene, const Camera& camera, gfx::GpuTimer& timer,
                     bool useJitter, const char* scopeLabel) {
    gfx::GpuScope scope(timer, scopeLabel);

    // This frame writes into the surface that was history last frame.
    color_.swap();
    depthInfo_.swap();
    fbo_.attachColor(0, color_.current());
    fbo_.attachColor(3, depthInfo_.current());
    fbo_.bind();
    glViewport(0, 0, width_, height_);
    glEnable(GL_DEPTH_TEST);
    // Reverse-Z: clear to 0 (far) and keep the greater depth.
    glDepthFunc(GL_GREATER);
    glClearDepth(0.0);
    glDisable(GL_BLEND);

    const float clearColor[4] = {0.02f, 0.03f, 0.05f, 1.f};
    const float clearVelocity[4] = {0.f, 0.f, 0.f, 0.f};
    glClearNamedFramebufferfv(fbo_.id, GL_COLOR, 0, clearColor);
    glClearNamedFramebufferfv(fbo_.id, GL_COLOR, 1, clearVelocity);
    glClearNamedFramebufferfv(fbo_.id, GL_COLOR, 2, clearVelocity);
    // Sky: infinitely far in both frames, so the disocclusion test sees no
    // change and leaves the history alone.
    const float clearDepthInfo[4] = {1e6f, 1e6f, 0.f, 0.f};
    glClearNamedFramebufferfv(fbo_.id, GL_COLOR, 3, clearDepthInfo);
    const float clearDepth = 0.f;
    glClearNamedFramebufferfv(fbo_.id, GL_DEPTH, 0, &clearDepth);

    program_.bind();
    const glm::mat4 curVPJittered = useJitter ? camera.viewProjJittered() : camera.viewProj();
    const glm::mat4 curVP = camera.viewProj();
    const glm::mat4 prevVP = camera.prevViewProj();
    program_.setMat4("uCurVPJittered", glm::value_ptr(curVPJittered));
    program_.setMat4("uCurVP", glm::value_ptr(curVP));
    program_.setMat4("uPrevVP", glm::value_ptr(prevVP));
    program_.set("uAlbedoTex", 0);
    program_.set("uMipBias", mipBias_);
    program_.set("uReactiveScale", reactiveScale_);

    for (const Instance& inst : scene.instances()) {
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(inst.model));
        program_.setMat4("uModel", glm::value_ptr(inst.model));
        program_.setMat4("uPrevModel", glm::value_ptr(inst.prevModel));
        glProgramUniformMatrix3fv(program_.id(), glGetUniformLocation(program_.id(), "uNormalMatrix"),
                                  1, GL_FALSE, glm::value_ptr(normalMatrix));
        program_.set("uAlbedo", inst.albedo.x, inst.albedo.y, inst.albedo.z);
        program_.set("uPattern", inst.pattern);
        program_.set("uPatternScale", inst.patternScale);
        program_.set("uPatternFilter", patternFilter_ ? 1 : 0);

        const bool hasTexture = inst.albedoTexture >= 0 &&
                                inst.albedoTexture < static_cast<int>(scene.textures().size());
        program_.set("uHasAlbedoTex", hasTexture ? 1 : 0);
        if (hasTexture) glBindTextureUnit(0, scene.textures()[inst.albedoTexture]);

        const Mesh& mesh = scene.meshes()[inst.mesh];
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}

void GBuffer::destroy() {
    program_.destroy();
    color_.destroy();
    velocity_.destroy();
    reactive_.destroy();
    depthInfo_.destroy();
    depth_.destroy();
    fbo_.destroy();
}

}  // namespace renderer
