#pragma once

#include <string>
#include <vector>

#include "gfx/gl_common.h"

namespace gfx {

// Immutable-storage 2D texture. Every intermediate surface of the upscaler and
// the frame generator is one of these, so the helper also carries the metadata
// the passes need (size, format, mip count).
struct Texture2D {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int levels = 1;
    GLenum format = GL_RGBA16F;
    std::string name;

    void create(int w, int h, GLenum internalFormat, int mipLevels, const std::string& label);
    void destroy();
    // Recreates the texture only if the requested size or format differ.
    void ensure(int w, int h, GLenum internalFormat, int mipLevels, const std::string& label);

    void bindTexture(GLuint unit) const { glBindTextureUnit(unit, id); }
    void bindImage(GLuint unit, GLenum access, int level = 0) const {
        glBindImageTexture(unit, id, level, GL_FALSE, 0, access, format);
    }
    void clear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 0.f) const;
    void clearUint(unsigned v = 0) const;

    bool valid() const { return id != 0; }
};

// Immutable-storage 2D array texture, one mip level, linear filtering. The ML
// blend network keeps its feature maps in these: a layer of N channels is
// ceil(N / 4) RGBA16F layers, which is what lets a convolution run as mat4
// products over vec4s.
struct Texture2DArray {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int layers = 0;
    GLenum format = GL_RGBA16F;

    void ensure(int w, int h, int layerCount, GLenum internalFormat, const std::string& label);
    void destroy();
    void bindTexture(GLuint unit) const { glBindTextureUnit(unit, id); }
    void bindImage(GLuint unit, GLenum access) const {
        glBindImageTexture(unit, id, 0, GL_TRUE, 0, access, format);
    }
    bool valid() const { return id != 0; }
};

// Ping-pong pair: history-consuming passes read [1 - index] and write [index].
struct PingPong {
    Texture2D tex[2];
    int index = 0;

    void ensure(int w, int h, GLenum fmt, const std::string& label);
    void destroy();
    void swap() { index ^= 1; }
    Texture2D& current() { return tex[index]; }
    Texture2D& history() { return tex[index ^ 1]; }
    const Texture2D& current() const { return tex[index]; }
    const Texture2D& history() const { return tex[index ^ 1]; }
};

// Minimal FBO wrapper used by the G-buffer pass; everything after the G-buffer
// is compute and writes through image units instead.
struct Framebuffer {
    GLuint id = 0;
    std::vector<GLenum> drawBuffers;

    void create(const std::string& label);
    void destroy();
    void attachColor(int slot, const Texture2D& tex, int level = 0);
    void attachDepth(const Texture2D& tex, int level = 0);
    void finalizeAttachments();
    void bind() const;
    static void bindDefault(int width, int height);
};

}  // namespace gfx
