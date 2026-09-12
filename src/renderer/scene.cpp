#include "renderer/scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace renderer {
namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

Mesh uploadMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices,
                const std::string& label) {
    Mesh m;
    glCreateBuffers(1, &m.vbo);
    glNamedBufferStorage(m.vbo, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                         vertices.data(), 0);
    glCreateBuffers(1, &m.ibo);
    glNamedBufferStorage(m.ibo, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)),
                         indices.data(), 0);

    glCreateVertexArrays(1, &m.vao);
    glVertexArrayVertexBuffer(m.vao, 0, m.vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(m.vao, m.ibo);
    const GLuint attribs[3] = {0, 1, 2};
    const GLint sizes[3] = {3, 3, 2};
    const GLuint offsets[3] = {offsetof(Vertex, position), offsetof(Vertex, normal),
                               offsetof(Vertex, uv)};
    for (int i = 0; i < 3; ++i) {
        glEnableVertexArrayAttrib(m.vao, attribs[i]);
        glVertexArrayAttribFormat(m.vao, attribs[i], sizes[i], GL_FLOAT, GL_FALSE, offsets[i]);
        glVertexArrayAttribBinding(m.vao, attribs[i], 0);
    }
    m.indexCount = static_cast<GLsizei>(indices.size());
    gfx::objectLabel(GL_VERTEX_ARRAY, m.vao, label);
    return m;
}

}  // namespace

int Scene::addCube() {
    const glm::vec3 faceNormals[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const glm::vec3 faceTangents[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}};

    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    for (int f = 0; f < 6; ++f) {
        const glm::vec3 n = faceNormals[f];
        const glm::vec3 t = faceTangents[f];
        const glm::vec3 b = glm::cross(n, t);
        const unsigned base = static_cast<unsigned>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            const float u = (i == 1 || i == 2) ? 1.f : 0.f;
            const float v = (i >= 2) ? 1.f : 0.f;
            const glm::vec3 p = n * 0.5f + t * (u - 0.5f) + b * (v - 0.5f);
            vertices.push_back({p, n, {u, v}});
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    meshes_.push_back(uploadMesh(vertices, indices, "cube"));
    return static_cast<int>(meshes_.size()) - 1;
}

int Scene::addPlane() {
    const std::vector<Vertex> vertices = {
        {{-0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 0.f}},
        {{0.5f, 0.f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f}},
        {{-0.5f, 0.f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 1.f}},
    };
    const std::vector<unsigned> indices = {0, 1, 2, 0, 2, 3};
    meshes_.push_back(uploadMesh(vertices, indices, "plane"));
    return static_cast<int>(meshes_.size()) - 1;
}

void Scene::createProcedural() {
    destroy();
    procedural_ = true;
    const int cube = addCube();
    const int plane = addPlane();

    // Ground: a large checkerboard. Minification of a high-contrast pattern is
    // the classic aliasing stress case, and the first thing a temporal
    // upscaler has to get right.
    Instance ground;
    ground.mesh = plane;
    ground.model = glm::scale(glm::mat4(1.f), glm::vec3(40.f, 1.f, 40.f));
    ground.albedo = glm::vec3(0.85f);
    ground.pattern = 1;
    ground.patternScale = 60.f;
    instances_.push_back(ground);

    // Static pillars: thin, vertical geometry - what pixel locks exist for.
    for (int i = 0; i < 8; ++i) {
        const float a = static_cast<float>(i) / 8.f * 6.2831853f;
        Instance pillar;
        pillar.mesh = cube;
        pillar.model = glm::translate(glm::mat4(1.f), glm::vec3(std::cos(a) * 3.f, 1.5f, std::sin(a) * 3.f)) *
                       glm::scale(glm::mat4(1.f), glm::vec3(0.12f, 3.f, 0.12f));
        pillar.albedo = glm::vec3(0.9f, 0.85f, 0.75f);
        pillar.pattern = 2;
        pillar.patternScale = 24.f;
        instances_.push_back(pillar);
    }

    // Rotating boxes: object motion, so the velocity buffer is exercised
    // independently of the camera.
    for (int i = 0; i < 6; ++i) {
        Instance box;
        box.mesh = cube;
        box.albedo = glm::vec3(0.2f + 0.13f * i, 0.5f, 0.9f - 0.1f * i);
        box.pattern = 1;
        box.patternScale = 6.f;
        instances_.push_back(box);
    }

    // One fast orbiting object: the disocclusion and frame-generation stress
    // case. Anything that breaks, breaks here first.
    Instance fast;
    fast.mesh = cube;
    fast.albedo = glm::vec3(1.f, 0.35f, 0.1f);
    fast.pattern = 0;
    instances_.push_back(fast);

    update(0.0);
    // First frame has no history: make previous transforms match current ones
    // so frame 0 produces zero object motion instead of garbage.
    for (Instance& inst : instances_) inst.prevModel = inst.model;
}

void Scene::update(double t, bool recordHistory) {
    // glTF scenes are static geometry for now: only the camera moves, so the
    // per-instance transforms (and therefore prevModel) never change.
    if (!procedural_) return;

    const float time = static_cast<float>(t);
    size_t index = 1 + 8;  // ground + pillars are static

    for (int i = 0; i < 6 && index < instances_.size(); ++i, ++index) {
        Instance& box = instances_[index];
        if (recordHistory) box.prevModel = box.model;
        const float a = static_cast<float>(i) / 6.f * 6.2831853f + time * 0.4f;
        const float radius = 1.6f;
        box.model = glm::translate(glm::mat4(1.f),
                                   glm::vec3(std::cos(a) * radius, 0.6f + 0.35f * std::sin(time + i),
                                             std::sin(a) * radius)) *
                    glm::rotate(glm::mat4(1.f), time * (0.6f + 0.2f * i), glm::vec3(0.3f, 1.f, 0.2f)) *
                    glm::scale(glm::mat4(1.f), glm::vec3(0.55f));
    }

    if (index < instances_.size()) {
        Instance& fast = instances_[index];
        if (recordHistory) fast.prevModel = fast.model;
        const float a = time * 2.6f;
        fast.model = glm::translate(glm::mat4(1.f), glm::vec3(std::cos(a) * 2.4f, 1.2f, std::sin(a) * 2.4f)) *
                     glm::rotate(glm::mat4(1.f), time * 3.f, glm::vec3(0.f, 1.f, 0.f)) *
                     glm::scale(glm::mat4(1.f), glm::vec3(0.4f));
    }
}

void Scene::destroy() {
    for (Mesh& m : meshes_) {
        glDeleteVertexArrays(1, &m.vao);
        glDeleteBuffers(1, &m.vbo);
        glDeleteBuffers(1, &m.ibo);
    }
    meshes_.clear();
    for (GLuint tex : textures_) glDeleteTextures(1, &tex);
    textures_.clear();
    instances_.clear();
    procedural_ = false;
}

}  // namespace renderer
