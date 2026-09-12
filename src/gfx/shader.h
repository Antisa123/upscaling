#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "gfx/gl_common.h"

namespace gfx {

// A GL program built from files on disk. Shader sources support a simple
// `#include "relative/path.glsl"` directive (resolved against the shader
// directory) so passes can share code the way the FidelityFX headers do.
//
// Programs remember their source files' timestamps: calling reloadIfChanged()
// recompiles on the fly and keeps the old program if compilation fails, which
// makes tuning the upscaler passes bearable.
class Program {
public:
    Program() = default;
    ~Program();
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&& other) noexcept { *this = std::move(other); }
    Program& operator=(Program&& other) noexcept;

    // Graphics program from vertex + fragment shader file names.
    static Program graphics(const std::string& vertFile, const std::string& fragFile);
    // Compute program from a single file name.
    static Program compute(const std::string& compFile);

    // GL objects must be released while the context is still current, so
    // teardown is explicit rather than left to the destructor.
    void destroy();

    bool valid() const { return program_ != 0; }
    GLuint id() const { return program_; }
    void bind() const { glUseProgram(program_); }

    // Recompiles if any source file (including includes) changed on disk.
    // Returns true if a reload happened.
    bool reloadIfChanged();

    // Dispatches a compute program covering `w` x `h` pixels with the given
    // local group size.
    void dispatch(int w, int h, int localX = 8, int localY = 8) const;

    // Uniform setters by name; unknown names are silently ignored so debug
    // uniforms can be stripped from shaders without breaking the C++ side.
    void set(const char* name, int v) const;
    void set(const char* name, unsigned v) const;
    void set(const char* name, float v) const;
    void set(const char* name, int x, int y) const;
    void set(const char* name, float x, float y) const;
    void set(const char* name, float x, float y, float z) const;
    void setMat4(const char* name, const float* m) const;

    static void setShaderDirectory(const std::filesystem::path& dir);

private:
    struct Stage {
        GLenum type = 0;
        std::string file;
    };

    bool build();

    GLuint program_ = 0;
    std::vector<Stage> stages_;
    // All files that contributed to the last successful build (sources +
    // includes) with the timestamp they had at that point.
    std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> deps_;
};

}  // namespace gfx
