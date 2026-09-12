#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "gfx/gl_common.h"

namespace renderer {

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
};

// One drawable. The previous model matrix is the other half of the motion
// vector story: without it, only camera motion is captured and every moving
// object ghosts.
struct Instance {
    int mesh = 0;
    glm::mat4 model{1.f};
    glm::mat4 prevModel{1.f};
    glm::vec3 albedo{0.8f};
    // Selects a procedural surface pattern in the shader: 0 flat, 1 checker,
    // 2 fine stripes. High-frequency patterns are deliberate - they are the
    // cases where temporal upscaling either wins or falls apart.
    int pattern = 0;
    float patternScale = 8.f;
    // Index into Scene::textures(), or -1 for a flat albedo. glTF materials
    // set this; the procedural scene never does.
    int albedoTexture = -1;
};

// Test scene, either procedural or loaded from glTF. The passes only ever see
// meshes + instances, so both sources are interchangeable downstream.
class Scene {
public:
    void createProcedural();
    // Loads every triangle primitive in the file, flattening the node
    // hierarchy into world-space instances. `fitSize` rescales the scene so
    // its largest dimension is that many units (0 keeps the original units).
    bool loadGltf(const std::string& path, float fitSize = 12.f);
    void destroy();

    // Advances animation to absolute time `t` (seconds). `recordHistory`
    // rolls current transforms into previous ones; ground-truth capture
    // evaluates the scene at intermediate times and must not do that, or the
    // next frame's motion vectors would be measured against the wrong past.
    void update(double t, bool recordHistory = true);

    const std::vector<Mesh>& meshes() const { return meshes_; }
    const std::vector<Instance>& instances() const { return instances_; }
    const std::vector<GLuint>& textures() const { return textures_; }
    bool procedural() const { return procedural_; }

private:
    int addCube();
    int addPlane();

    std::vector<Mesh> meshes_;
    std::vector<Instance> instances_;
    std::vector<GLuint> textures_;
    bool procedural_ = false;
};

}  // namespace renderer
