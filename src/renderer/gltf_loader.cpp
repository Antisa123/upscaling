// glTF loading. Kept in its own translation unit because cgltf and stb_image
// are single-header libraries: their implementations are compiled exactly once,
// here, and nowhere else.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <vector>

#include "renderer/scene.h"

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
    const GLint sizes[3] = {3, 3, 2};
    const GLuint offsets[3] = {offsetof(Vertex, position), offsetof(Vertex, normal),
                               offsetof(Vertex, uv)};
    for (GLuint i = 0; i < 3; ++i) {
        glEnableVertexArrayAttrib(m.vao, i);
        glVertexArrayAttribFormat(m.vao, i, sizes[i], GL_FLOAT, GL_FALSE, offsets[i]);
        glVertexArrayAttribBinding(m.vao, i, 0);
    }
    m.indexCount = static_cast<GLsizei>(indices.size());
    gfx::objectLabel(GL_VERTEX_ARRAY, m.vao, label);
    return m;
}

// Uploads 8-bit RGBA pixels as sRGB with a full mip chain. Mips are not
// optional here: a minified unmipped albedo aliases so hard that it would
// dominate every quality metric in this project.
GLuint uploadTexture(const unsigned char* pixels, int width, int height, const std::string& label) {
    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    const int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(width, height))));
    glTextureStorage2D(tex, levels, GL_SRGB8_ALPHA8, width, height);
    glTextureSubImage2D(tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateTextureMipmap(tex);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic")) {
        GLfloat maxAniso = 1.f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.f, maxAniso));
    }
    gfx::objectLabel(GL_TEXTURE, tex, label);
    return tex;
}

GLuint loadImage(const cgltf_image* image, const std::filesystem::path& baseDir) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = nullptr;

    if (image->buffer_view && image->buffer_view->buffer->data) {
        // GLB, or a .gltf whose images were packed into the binary buffer.
        const unsigned char* src = static_cast<const unsigned char*>(image->buffer_view->buffer->data) +
                                   image->buffer_view->offset;
        pixels = stbi_load_from_memory(src, static_cast<int>(image->buffer_view->size), &w, &h,
                                       &channels, 4);
    } else if (image->uri && std::strncmp(image->uri, "data:", 5) != 0) {
        // cgltf leaves percent-encoding in the URI; decode it in place.
        std::string uri = image->uri;
        cgltf_decode_uri(uri.data());
        uri.resize(std::strlen(uri.c_str()));
        pixels = stbi_load((baseDir / uri).string().c_str(), &w, &h, &channels, 4);
    }

    if (!pixels) {
        std::fprintf(stderr, "[gltf] could not decode image '%s'\n", image->uri ? image->uri : "<embedded>");
        return 0;
    }
    const GLuint tex = uploadTexture(pixels, w, h, image->uri ? image->uri : "gltf.image");
    stbi_image_free(pixels);
    return tex;
}

glm::mat4 nodeWorld(const cgltf_node* node) {
    float m[16];
    cgltf_node_transform_world(node, m);
    return glm::make_mat4(m);
}

}  // namespace

bool Scene::loadGltf(const std::string& path, float fitSize) {
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "[gltf] failed to parse %s\n", path.c_str());
        return false;
    }
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "[gltf] failed to load buffers for %s\n", path.c_str());
        cgltf_free(data);
        return false;
    }
    if (cgltf_validate(data) != cgltf_result_success)
        std::fprintf(stderr, "[gltf] warning: %s failed validation, loading anyway\n", path.c_str());

    destroy();
    procedural_ = false;

    const std::filesystem::path baseDir = std::filesystem::path(path).parent_path();

    // Textures are shared between materials, so decode each image once.
    std::unordered_map<const cgltf_image*, GLuint> imageCache;
    auto textureIndexFor = [&](const cgltf_texture* texture) -> int {
        if (!texture || !texture->image) return -1;
        auto it = imageCache.find(texture->image);
        GLuint id = 0;
        if (it != imageCache.end()) {
            id = it->second;
        } else {
            id = loadImage(texture->image, baseDir);
            imageCache.emplace(texture->image, id);
        }
        if (id == 0) return -1;
        for (size_t i = 0; i < textures_.size(); ++i)
            if (textures_[i] == id) return static_cast<int>(i);
        textures_.push_back(id);
        return static_cast<int>(textures_.size()) - 1;
    };

    // (primitive -> mesh index), so a mesh referenced by several nodes is
    // uploaded once and drawn as several instances.
    std::unordered_map<const cgltf_primitive*, int> primitiveCache;
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

    for (size_t n = 0; n < data->nodes_count; ++n) {
        const cgltf_node* node = &data->nodes[n];
        if (!node->mesh) continue;
        const glm::mat4 world = nodeWorld(node);

        for (size_t p = 0; p < node->mesh->primitives_count; ++p) {
            const cgltf_primitive* prim = &node->mesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            int meshIndex = -1;
            auto cached = primitiveCache.find(prim);
            if (cached != primitiveCache.end()) {
                meshIndex = cached->second;
            } else {
                const cgltf_accessor* positions = nullptr;
                const cgltf_accessor* normals = nullptr;
                const cgltf_accessor* uvs = nullptr;
                for (size_t a = 0; a < prim->attributes_count; ++a) {
                    const cgltf_attribute& attr = prim->attributes[a];
                    if (attr.type == cgltf_attribute_type_position) positions = attr.data;
                    else if (attr.type == cgltf_attribute_type_normal) normals = attr.data;
                    else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvs = attr.data;
                }
                if (!positions) continue;

                std::vector<Vertex> vertices(positions->count);
                for (size_t v = 0; v < positions->count; ++v) {
                    cgltf_accessor_read_float(positions, v, &vertices[v].position.x, 3);
                    if (normals) cgltf_accessor_read_float(normals, v, &vertices[v].normal.x, 3);
                    else vertices[v].normal = glm::vec3(0.f, 1.f, 0.f);
                    if (uvs) cgltf_accessor_read_float(uvs, v, &vertices[v].uv.x, 2);
                }

                std::vector<unsigned> indices;
                if (prim->indices) {
                    indices.resize(prim->indices->count);
                    for (size_t i = 0; i < prim->indices->count; ++i)
                        indices[i] = static_cast<unsigned>(cgltf_accessor_read_index(prim->indices, i));
                } else {
                    indices.resize(vertices.size());
                    for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<unsigned>(i);
                }
                if (indices.empty()) continue;

                meshes_.push_back(uploadMesh(vertices, indices,
                                             node->mesh->name ? node->mesh->name : "gltf.mesh"));
                meshIndex = static_cast<int>(meshes_.size()) - 1;
                primitiveCache.emplace(prim, meshIndex);

                for (const Vertex& v : vertices) {
                    const glm::vec3 world3 = glm::vec3(world * glm::vec4(v.position, 1.f));
                    boundsMin = glm::min(boundsMin, world3);
                    boundsMax = glm::max(boundsMax, world3);
                }
            }
            if (meshIndex < 0) continue;

            Instance inst;
            inst.mesh = meshIndex;
            inst.model = world;
            inst.prevModel = world;
            inst.pattern = 0;  // real textures replace the procedural patterns
            if (const cgltf_material* mat = prim->material) {
                if (mat->has_pbr_metallic_roughness) {
                    const cgltf_pbr_metallic_roughness& pbr = mat->pbr_metallic_roughness;
                    inst.albedo = glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                            pbr.base_color_factor[2]);
                    inst.albedoTexture = textureIndexFor(pbr.base_color_texture.texture);
                }
            }
            instances_.push_back(inst);
        }
    }

    cgltf_free(data);

    if (instances_.empty()) {
        std::fprintf(stderr, "[gltf] %s contained no triangle geometry\n", path.c_str());
        return false;
    }

    // Scenes come in wildly different units (Sponza is ~30 units wide, most
    // sample models are ~1). Normalising to a known size means the camera path
    // and the near/far planes do not have to be retuned per asset.
    if (fitSize > 0.f) {
        const glm::vec3 extent = boundsMax - boundsMin;
        const float largest = std::max(extent.x, std::max(extent.y, extent.z));
        if (largest > 0.f) {
            const float k = fitSize / largest;
            const glm::vec3 centre((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y,
                                   (boundsMin.z + boundsMax.z) * 0.5f);
            const glm::mat4 fit = glm::scale(glm::mat4(1.f), glm::vec3(k)) *
                                  glm::translate(glm::mat4(1.f), -centre);
            for (Instance& inst : instances_) {
                inst.model = fit * inst.model;
                inst.prevModel = inst.model;
            }
        }
    }

    std::printf("[gltf] %s: %zu meshes, %zu instances, %zu textures\n", path.c_str(), meshes_.size(),
                instances_.size(), textures_.size());
    return true;
}

}  // namespace renderer
