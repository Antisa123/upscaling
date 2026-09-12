#include "gfx/texture.h"

#include <cstdio>

namespace gfx {

void Texture2D::create(int w, int h, GLenum internalFormat, int mipLevels,
                       const std::string& label) {
    destroy();
    width = w;
    height = h;
    levels = mipLevels < 1 ? 1 : mipLevels;
    format = internalFormat;
    name = label;

    glCreateTextures(GL_TEXTURE_2D, 1, &id);
    glTextureStorage2D(id, levels, internalFormat, w, h);
    glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Integer formats cannot be linearly filtered.
    const bool isInteger = internalFormat == GL_R32UI || internalFormat == GL_RG32UI ||
                           internalFormat == GL_RGBA32UI || internalFormat == GL_R32I;
    const GLenum filter = isInteger ? GL_NEAREST : GL_LINEAR;
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, filter);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER,
                        levels > 1 ? (isInteger ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST)
                                   : filter);
    objectLabel(GL_TEXTURE, id, label);
}

void Texture2D::destroy() {
    if (id) glDeleteTextures(1, &id);
    id = 0;
    width = height = 0;
}

void Texture2D::ensure(int w, int h, GLenum internalFormat, int mipLevels,
                       const std::string& label) {
    if (id && width == w && height == h && format == internalFormat && levels == mipLevels) return;
    create(w, h, internalFormat, mipLevels, label);
}

void Texture2D::clear(float r, float g, float b, float a) const {
    const float v[4] = {r, g, b, a};
    for (int l = 0; l < levels; ++l) glClearTexImage(id, l, GL_RGBA, GL_FLOAT, v);
}

void Texture2D::clearUint(unsigned value) const {
    const unsigned v[4] = {value, value, value, value};
    // The format has to match the texture's component count: clearing a single
    // channel image with GL_RGBA_INTEGER is an invalid operation, and the clear
    // silently does not happen -- which, for the frame generator's scatter
    // targets, means last frame's field survives into this one.
    const GLenum layout = (format == GL_R32UI || format == GL_R32I)  ? GL_RED_INTEGER
                          : (format == GL_RG32UI)                    ? GL_RG_INTEGER
                                                                     : GL_RGBA_INTEGER;
    for (int l = 0; l < levels; ++l) glClearTexImage(id, l, layout, GL_UNSIGNED_INT, v);
}

void PingPong::ensure(int w, int h, GLenum fmt, const std::string& label) {
    tex[0].ensure(w, h, fmt, 1, label + "[0]");
    tex[1].ensure(w, h, fmt, 1, label + "[1]");
}

void PingPong::destroy() {
    tex[0].destroy();
    tex[1].destroy();
}

void Framebuffer::create(const std::string& label) {
    destroy();
    glCreateFramebuffers(1, &id);
    objectLabel(GL_FRAMEBUFFER, id, label);
}

void Framebuffer::destroy() {
    if (id) glDeleteFramebuffers(1, &id);
    id = 0;
    drawBuffers.clear();
}

void Framebuffer::attachColor(int slot, const Texture2D& tex, int level) {
    const GLenum attachment = GL_COLOR_ATTACHMENT0 + slot;
    glNamedFramebufferTexture(id, attachment, tex.id, level);
    if (static_cast<int>(drawBuffers.size()) <= slot) drawBuffers.resize(slot + 1, GL_NONE);
    drawBuffers[slot] = attachment;
}

void Framebuffer::attachDepth(const Texture2D& tex, int level) {
    glNamedFramebufferTexture(id, GL_DEPTH_ATTACHMENT, tex.id, level);
}

void Framebuffer::finalizeAttachments() {
    glNamedFramebufferDrawBuffers(id, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    const GLenum status = glCheckNamedFramebufferStatus(id, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[gl] framebuffer incomplete: 0x%x\n", status);
}

void Framebuffer::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, id); }

void Framebuffer::bindDefault(int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
}

}  // namespace gfx
