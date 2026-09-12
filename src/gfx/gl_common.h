#pragma once

#include <epoxy/gl.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace gfx {

// Installs KHR_debug output. Severity notification is dropped, everything else
// is printed; errors additionally trigger a breakpoint-friendly message.
void installDebugCallback();

inline void objectLabel(GLenum type, GLuint id, const std::string& name) {
    if (epoxy_has_gl_extension("GL_KHR_debug"))
        glObjectLabel(type, id, static_cast<GLsizei>(name.size()), name.c_str());
}

}  // namespace gfx
