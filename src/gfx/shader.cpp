#include "gfx/shader.h"

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace gfx {
namespace {

std::filesystem::path g_shaderDir = FSR3LITE_SHADER_DIR;

bool readFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// Resolves `#include "file"` recursively. Each included file is emitted once;
// `deps` collects every file touched so hot reload can watch all of them.
bool preprocess(const std::filesystem::path& path, std::string& out,
                std::vector<std::filesystem::path>& deps,
                std::unordered_set<std::string>& seen, int depth = 0) {
    if (depth > 16) {
        std::fprintf(stderr, "[shader] include nesting too deep at %s\n", path.c_str());
        return false;
    }
    if (!seen.insert(path.string()).second) return true;  // already included

    std::string src;
    if (!readFile(path, src)) {
        std::fprintf(stderr, "[shader] cannot open %s\n", path.c_str());
        return false;
    }
    deps.push_back(path);

    std::istringstream lines(src);
    std::string line;
    int lineNo = 0;
    while (std::getline(lines, line)) {
        ++lineNo;
        const size_t hash = line.find_first_not_of(" \t");
        if (hash != std::string::npos && line.compare(hash, 8, "#include") == 0) {
            const size_t first = line.find('"', hash);
            const size_t last = line.find('"', first + 1);
            if (first == std::string::npos || last == std::string::npos) {
                std::fprintf(stderr, "[shader] malformed #include in %s:%d\n", path.c_str(), lineNo);
                return false;
            }
            const std::string rel = line.substr(first + 1, last - first - 1);
            if (!preprocess(g_shaderDir / rel, out, deps, seen, depth + 1)) return false;
            // Restore line numbering for the including file. GLSL only accepts
            // a file *number*, so we use 0 and rely on the log prefix instead.
            out += "#line " + std::to_string(lineNo + 1) + " 0\n";
        } else {
            out += line;
            out += '\n';
        }
    }
    return true;
}

GLuint compileStage(GLenum type, const std::filesystem::path& path,
                    std::vector<std::filesystem::path>& deps, const std::string& defines) {
    std::string source;
    std::unordered_set<std::string> seen;
    if (!preprocess(path, source, deps, seen)) return 0;
    if (!defines.empty()) {
        const size_t version = source.find("#version");
        const size_t eol = version == std::string::npos ? std::string::npos : source.find('\n', version);
        if (eol != std::string::npos) source.insert(eol + 1, defines + "\n#line 2 0\n");
    }

    const GLuint shader = glCreateShader(type);
    const char* csrc = source.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] %s failed to compile:\n%s\n", path.c_str(), log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    objectLabel(GL_SHADER, shader, path.filename().string());
    return shader;
}

}  // namespace

void Program::setShaderDirectory(const std::filesystem::path& dir) { g_shaderDir = dir; }

Program::~Program() {
    // Intentionally no GL call here: by the time static/stack teardown runs,
    // the context may already be gone. Call destroy() explicitly.
    program_ = 0;
}

void Program::destroy() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
    deps_.clear();
}

Program& Program::operator=(Program&& other) noexcept {
    if (this != &other) {
        if (program_) glDeleteProgram(program_);
        program_ = other.program_;
        stages_ = std::move(other.stages_);
        defines_ = std::move(other.defines_);
        deps_ = std::move(other.deps_);
        other.program_ = 0;
    }
    return *this;
}

Program Program::graphics(const std::string& vertFile, const std::string& fragFile) {
    Program p;
    p.stages_ = {{GL_VERTEX_SHADER, vertFile}, {GL_FRAGMENT_SHADER, fragFile}};
    p.build();
    return p;
}

Program Program::compute(const std::string& compFile, const std::string& defines) {
    Program p;
    p.stages_ = {{GL_COMPUTE_SHADER, compFile}};
    p.defines_ = defines;
    p.build();
    return p;
}

bool Program::build() {
    std::vector<std::filesystem::path> deps;
    std::vector<GLuint> shaders;
    bool ok = true;

    for (const Stage& stage : stages_) {
        const GLuint s = compileStage(stage.type, g_shaderDir / stage.file, deps, defines_);
        if (!s) {
            ok = false;
            break;
        }
        shaders.push_back(s);
    }

    GLuint prog = 0;
    if (ok) {
        prog = glCreateProgram();
        for (GLuint s : shaders) glAttachShader(prog, s);
        glLinkProgram(prog);

        GLint linked = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<size_t>(len), '\0');
            glGetProgramInfoLog(prog, len, nullptr, log.data());
            std::fprintf(stderr, "[shader] link failed (%s):\n%s\n", stages_[0].file.c_str(),
                         log.c_str());
            glDeleteProgram(prog);
            prog = 0;
            ok = false;
        }
    }
    for (GLuint s : shaders) glDeleteShader(s);

    if (!ok) return false;

    if (program_) glDeleteProgram(program_);
    program_ = prog;
    objectLabel(GL_PROGRAM, program_, stages_[0].file);

    deps_.clear();
    for (const auto& d : deps) {
        std::error_code ec;
        deps_.emplace_back(d, std::filesystem::last_write_time(d, ec));
    }
    return true;
}

bool Program::reloadIfChanged() {
    bool changed = false;
    for (const auto& [path, stamp] : deps_) {
        std::error_code ec;
        const auto now = std::filesystem::last_write_time(path, ec);
        if (!ec && now != stamp) {
            changed = true;
            break;
        }
    }
    if (!changed) return false;

    std::printf("[shader] reloading %s\n", stages_[0].file.c_str());
    // build() keeps the previous program alive if compilation fails, so a typo
    // in a shader never takes the application down.
    return build();
}

void Program::dispatch(int w, int h, int localX, int localY) const {
    const GLuint gx = static_cast<GLuint>((w + localX - 1) / localX);
    const GLuint gy = static_cast<GLuint>((h + localY - 1) / localY);
    glDispatchCompute(gx, gy, 1);
}

void Program::set(const char* name, int v) const {
    glProgramUniform1i(program_, glGetUniformLocation(program_, name), v);
}
void Program::set(const char* name, unsigned v) const {
    glProgramUniform1ui(program_, glGetUniformLocation(program_, name), v);
}
void Program::set(const char* name, float v) const {
    glProgramUniform1f(program_, glGetUniformLocation(program_, name), v);
}
void Program::set(const char* name, int x, int y) const {
    glProgramUniform2i(program_, glGetUniformLocation(program_, name), x, y);
}

void Program::set(const char* name, float x, float y) const {
    glProgramUniform2f(program_, glGetUniformLocation(program_, name), x, y);
}
void Program::set(const char* name, float x, float y, float z) const {
    glProgramUniform3f(program_, glGetUniformLocation(program_, name), x, y, z);
}
void Program::setMat4(const char* name, const float* m) const {
    glProgramUniformMatrix4fv(program_, glGetUniformLocation(program_, name), 1, GL_FALSE, m);
}

void installDebugCallback() {
    if (!epoxy_has_gl_extension("GL_KHR_debug")) return;
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(
        [](GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* msg, const void*) {
            if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
            const char* kind = type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "warn";
            std::fprintf(stderr, "[gl:%s] %s\n", kind, msg);
        },
        nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                          GL_FALSE);
}

}  // namespace gfx
