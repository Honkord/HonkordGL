/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GpuShaderCompiler implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/Video.h>

#include <cstring>
#include <vector>

#if HONKORDGL_PLATFORM_ANDROID
#include <GLES2/gl2.h>
#elif HONKORDGL_PLATFORM_APPLE
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#else
#if HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#if !HONKORDGL_PLATFORM_WINDOWS
#include <GL/glx.h>
#endif
#endif

namespace HonkordGL::Graphics {

#if !HONKORDGL_PLATFORM_ANDROID && !HONKORDGL_PLATFORM_APPLE

using PFN_glCreateShader = GLuint (*)(GLenum);
using PFN_glShaderSource = void (*)(GLuint, GLsizei, const GLchar * const *, const GLint *);
using PFN_glCompileShader = void (*)(GLuint);
using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteShader = void (*)(GLuint);
using PFN_glCreateProgram = GLuint (*)(void);
using PFN_glAttachShader = void (*)(GLuint, GLuint);
using PFN_glBindAttribLocation = void (*)(GLuint, GLuint, const GLchar *);
using PFN_glLinkProgram = void (*)(GLuint);
using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteProgram = void (*)(GLuint);

static PFN_glCreateShader s_glCreateShader{};
static PFN_glShaderSource s_glShaderSource{};
static PFN_glCompileShader s_glCompileShader{};
static PFN_glGetShaderiv s_glGetShaderiv{};
static PFN_glGetShaderInfoLog s_glGetShaderInfoLog{};
static PFN_glDeleteShader s_glDeleteShader{};
static PFN_glCreateProgram s_glCreateProgram{};
static PFN_glAttachShader s_glAttachShader{};
static PFN_glBindAttribLocation s_glBindAttribLocation{};
static PFN_glLinkProgram s_glLinkProgram{};
static PFN_glGetProgramiv s_glGetProgramiv{};
static PFN_glGetProgramInfoLog s_glGetProgramInfoLog{};
static PFN_glDeleteProgram s_glDeleteProgram{};

static void * LoadProc(const char * name) noexcept
{
#if HONKORDGL_PLATFORM_WINDOWS
    void * p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p)
        return p;
    return reinterpret_cast<void *>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));
#else
    return reinterpret_cast<void *>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#endif
}
static bool LoadShaderProcs() noexcept
{
    if (s_glCreateShader)
        return true;
    s_glCreateShader = reinterpret_cast<PFN_glCreateShader>(LoadProc("glCreateShader"));
    s_glShaderSource = reinterpret_cast<PFN_glShaderSource>(LoadProc("glShaderSource"));
    s_glCompileShader = reinterpret_cast<PFN_glCompileShader>(LoadProc("glCompileShader"));
    s_glGetShaderiv = reinterpret_cast<PFN_glGetShaderiv>(LoadProc("glGetShaderiv"));
    s_glGetShaderInfoLog = reinterpret_cast<PFN_glGetShaderInfoLog>(LoadProc("glGetShaderInfoLog"));
    s_glDeleteShader = reinterpret_cast<PFN_glDeleteShader>(LoadProc("glDeleteShader"));
    s_glCreateProgram = reinterpret_cast<PFN_glCreateProgram>(LoadProc("glCreateProgram"));
    s_glAttachShader = reinterpret_cast<PFN_glAttachShader>(LoadProc("glAttachShader"));
    s_glBindAttribLocation = reinterpret_cast<PFN_glBindAttribLocation>(LoadProc("glBindAttribLocation"));
    s_glLinkProgram = reinterpret_cast<PFN_glLinkProgram>(LoadProc("glLinkProgram"));
    s_glGetProgramiv = reinterpret_cast<PFN_glGetProgramiv>(LoadProc("glGetProgramiv"));
    s_glGetProgramInfoLog = reinterpret_cast<PFN_glGetProgramInfoLog>(LoadProc("glGetProgramInfoLog"));
    s_glDeleteProgram = reinterpret_cast<PFN_glDeleteProgram>(LoadProc("glDeleteProgram"));
    return s_glCreateShader && s_glShaderSource && s_glCompileShader && s_glGetShaderiv && s_glGetShaderInfoLog
        && s_glDeleteShader && s_glCreateProgram && s_glAttachShader && s_glLinkProgram && s_glGetProgramiv
        && s_glGetProgramInfoLog && s_glDeleteProgram;
}

#endif // !HONKORDGL_PLATFORM_ANDROID && !HONKORDGL_PLATFORM_APPLE

namespace {

void ClearLog(char * infoLog, std::size_t infoLogCap) noexcept
{
    if (infoLog && infoLogCap > 0)
        infoLog[0] = '\0';
}

void AppendToLog(char * infoLog, std::size_t infoLogCap, const char * chunk) noexcept
{
    if (!infoLog || infoLogCap == 0 || !chunk || chunk[0] == '\0')
        return;
    const std::size_t have = std::strlen(infoLog);
    const std::size_t maxAppend = infoLogCap - 1u - have;
    if (maxAppend == 0)
        return;
    std::strncat(infoLog, chunk, maxAppend);
}

#if !HONKORDGL_PLATFORM_ANDROID && !HONKORDGL_PLATFORM_APPLE

static void FetchShaderLog(GLuint shader, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    s_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    s_glGetShaderInfoLog(shader, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}
static void FetchProgramLog(GLuint prog, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    s_glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    s_glGetProgramInfoLog(prog, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}

#endif // desktop GL loader path

#if HONKORDGL_PLATFORM_APPLE

static void FetchShaderLog(GLuint shader, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    glGetShaderInfoLog(shader, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}
static void FetchProgramLog(GLuint prog, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    glGetProgramInfoLog(prog, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}

#endif

#if HONKORDGL_PLATFORM_ANDROID

static void FetchShaderLog(GLuint shader, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    glGetShaderInfoLog(shader, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}
static void FetchProgramLog(GLuint prog, char * infoLog, std::size_t infoLogCap) noexcept
{
    if (!infoLog || infoLogCap == 0)
        return;
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0)
        return;
    std::vector<char> tmp(static_cast<std::size_t>(len));
    GLsizei got = 0;
    glGetProgramInfoLog(prog, len, &got, tmp.data());
    AppendToLog(infoLog, infoLogCap, tmp.data());
}

#endif

bool BackendAllowsGlsl(ApplicationContextSettings const * ctx) noexcept
{
    if (!ctx)
        return true;
    const int ar = ctx->active_renderer;
    if (ar == static_cast<int>(Renderers::VULKAN) || ar == static_cast<int>(Renderers::METAL))
        return false;
    return true;
}

} // namespace

#if !HONKORDGL_PLATFORM_ANDROID && !HONKORDGL_PLATFORM_APPLE
int GpuShaderLoadCompilerProcs() noexcept
{
    return LoadShaderProcs() ? static_cast<int>(GpuShaderCompileError::OK)
                             : static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
}
#endif

#if HONKORDGL_PLATFORM_ANDROID || HONKORDGL_PLATFORM_APPLE
int GpuShaderLoadCompilerProcs() noexcept
{
    return static_cast<int>(GpuShaderCompileError::OK);
}
#endif

std::string GpuShaderPreamble(GpuShaderLanguage lang, GpuShaderStage stage)
{
    switch (lang) {
    case GpuShaderLanguage::CoreProfile330:
        return std::string("#version 330 core\n");
    case GpuShaderLanguage::EmbeddedProfile2:
        if (stage == GpuShaderStage::Fragment)
            return std::string("precision mediump float;\n");
        return std::string();
    }
    return std::string();
}
std::string GpuShaderWrapBody(GpuShaderLanguage lang, GpuShaderStage stage, const char * body)
{
    std::string s = GpuShaderPreamble(lang, stage);
    if (body)
        s += body;
    return s;
}
std::string GpuShaderGeneratedTexturedQuadVertexCore330()
{
    return std::string(R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)");
}
std::string GpuShaderGeneratedTexturedQuadFragmentCore330()
{
    return std::string(R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  o_color = texture(u_tex, vec2(v_uv.x, 1.0 - v_uv.y));
}
)");
}
std::string GpuShaderGeneratedSolidColorFragmentCore330()
{
    return std::string(R"(#version 330 core
uniform vec4 u_color;
out vec4 o_color;
void main() {
  o_color = u_color;
}
)");
}
int GpuShaderCompileProgram(
    ApplicationContextSettings const * ctx,
    const char * vertexGlsl,
    const char * fragmentGlsl,
    unsigned int * outProgram,
    char * infoLog,
    std::size_t infoLogCap,
    const int * attribLocations,
    const char * const * attribNames,
    int attribCount) noexcept
{
    ClearLog(infoLog, infoLogCap);

    if (!vertexGlsl || !fragmentGlsl || !outProgram) {
        SetInternalApiError("GpuShaderCompileProgram: null source or outProgram.");
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    }
    *outProgram = 0;

    if (!BackendAllowsGlsl(ctx)) {
        SetInternalApiError("GpuShaderCompileProgram: Vulkan/Metal active; use API-specific shaders.");
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    }

#if HONKORDGL_PLATFORM_ANDROID
    if (attribCount > 0 && (!attribLocations || !attribNames)) {
        SetInternalApiError("GpuShaderCompileProgram: attrib bindings require locations and names.");
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    }

    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs)
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    glShaderSource(vs, 1, &vertexGlsl, nullptr);
    glCompileShader(vs);
    GLint vok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vok);
    if (!vok) {
        FetchShaderLog(vs, infoLog, infoLogCap);
        glDeleteShader(vs);
        SetInternalApiError("GpuShaderCompileProgram: vertex compile failed.");
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    }

    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        glDeleteShader(vs);
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }
    glShaderSource(fs, 1, &fragmentGlsl, nullptr);
    glCompileShader(fs);
    GLint fok = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fok);
    if (!fok) {
        FetchShaderLog(fs, infoLog, infoLogCap);
        glDeleteShader(vs);
        glDeleteShader(fs);
        SetInternalApiError("GpuShaderCompileProgram: fragment compile failed.");
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    for (int i = 0; i < attribCount; ++i) {
        if (attribNames[i])
            glBindAttribLocation(prog, static_cast<GLuint>(attribLocations[i]), attribNames[i]);
    }
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint lok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &lok);
    if (!lok) {
        FetchProgramLog(prog, infoLog, infoLogCap);
        glDeleteProgram(prog);
        SetInternalApiError("GpuShaderCompileProgram: link failed.");
        return static_cast<int>(GpuShaderCompileError::LINK_FAILED);
    }

    *outProgram = prog;
    return static_cast<int>(GpuShaderCompileError::OK);

#elif HONKORDGL_PLATFORM_APPLE

    if (attribCount > 0 && (!attribLocations || !attribNames)) {
        SetInternalApiError("GpuShaderCompileProgram: attrib bindings require locations and names.");
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    }

    const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs)
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    glShaderSource(vs, 1, &vertexGlsl, nullptr);
    glCompileShader(vs);
    GLint vok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vok);
    if (!vok) {
        FetchShaderLog(vs, infoLog, infoLogCap);
        glDeleteShader(vs);
        SetInternalApiError("GpuShaderCompileProgram: vertex compile failed.");
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    }

    const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        glDeleteShader(vs);
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }
    glShaderSource(fs, 1, &fragmentGlsl, nullptr);
    glCompileShader(fs);
    GLint fok = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fok);
    if (!fok) {
        FetchShaderLog(fs, infoLog, infoLogCap);
        glDeleteShader(vs);
        glDeleteShader(fs);
        SetInternalApiError("GpuShaderCompileProgram: fragment compile failed.");
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    for (int i = 0; i < attribCount; ++i) {
        if (attribNames[i])
            glBindAttribLocation(prog, static_cast<GLuint>(attribLocations[i]), attribNames[i]);
    }
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint lok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &lok);
    if (!lok) {
        FetchProgramLog(prog, infoLog, infoLogCap);
        glDeleteProgram(prog);
        SetInternalApiError("GpuShaderCompileProgram: link failed.");
        return static_cast<int>(GpuShaderCompileError::LINK_FAILED);
    }

    *outProgram = prog;
    return static_cast<int>(GpuShaderCompileError::OK);

#else

    if (attribCount > 0 && (!attribLocations || !attribNames)) {
        SetInternalApiError("GpuShaderCompileProgram: attrib bindings require locations and names.");
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    }

    if (!LoadShaderProcs()) {
        SetInternalApiError("GpuShaderCompileProgram: failed to load GL entry points.");
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    }

    const GLuint vs = s_glCreateShader(GL_VERTEX_SHADER);
    if (!vs)
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    s_glShaderSource(vs, 1, &vertexGlsl, nullptr);
    s_glCompileShader(vs);
    GLint vok = 0;
    s_glGetShaderiv(vs, GL_COMPILE_STATUS, &vok);
    if (!vok) {
        FetchShaderLog(vs, infoLog, infoLogCap);
        s_glDeleteShader(vs);
        SetInternalApiError("GpuShaderCompileProgram: vertex compile failed.");
        return static_cast<int>(GpuShaderCompileError::VERTEX_COMPILE_FAILED);
    }

    const GLuint fs = s_glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        s_glDeleteShader(vs);
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }
    s_glShaderSource(fs, 1, &fragmentGlsl, nullptr);
    s_glCompileShader(fs);
    GLint fok = 0;
    s_glGetShaderiv(fs, GL_COMPILE_STATUS, &fok);
    if (!fok) {
        FetchShaderLog(fs, infoLog, infoLogCap);
        s_glDeleteShader(vs);
        s_glDeleteShader(fs);
        SetInternalApiError("GpuShaderCompileProgram: fragment compile failed.");
        return static_cast<int>(GpuShaderCompileError::FRAGMENT_COMPILE_FAILED);
    }

    const GLuint prog = s_glCreateProgram();
    s_glAttachShader(prog, vs);
    s_glAttachShader(prog, fs);
    if (s_glBindAttribLocation && attribCount > 0 && attribLocations && attribNames) {
        for (int i = 0; i < attribCount; ++i) {
            if (attribNames[i])
                s_glBindAttribLocation(prog, static_cast<GLuint>(attribLocations[i]), attribNames[i]);
        }
    }
    s_glLinkProgram(prog);
    s_glDeleteShader(vs);
    s_glDeleteShader(fs);
    GLint lok = 0;
    s_glGetProgramiv(prog, GL_LINK_STATUS, &lok);
    if (!lok) {
        FetchProgramLog(prog, infoLog, infoLogCap);
        s_glDeleteProgram(prog);
        SetInternalApiError("GpuShaderCompileProgram: link failed.");
        return static_cast<int>(GpuShaderCompileError::LINK_FAILED);
    }

    *outProgram = prog;
    return static_cast<int>(GpuShaderCompileError::OK);

#endif
}

namespace 
{
GLenum GpuPipelineStageToGLenum(GpuShaderPipelineStage stage, bool * unsupported) noexcept
{
    *unsupported = false;
    switch (stage) {
    case GpuShaderPipelineStage::Vertex:
        return GL_VERTEX_SHADER;
    case GpuShaderPipelineStage::Fragment:
        return GL_FRAGMENT_SHADER;
    case GpuShaderPipelineStage::Geometry:
#if defined(GL_GEOMETRY_SHADER)
        return GL_GEOMETRY_SHADER;
#else
        *unsupported = true;
        return 0;
#endif
    }
    *unsupported = true;
    return 0;
}

} // namespace

#if HONKORDGL_PLATFORM_ANDROID || HONKORDGL_PLATFORM_APPLE

int GpuShaderCreateObject(
    ApplicationContextSettings const * ctx,
    GpuShaderPipelineStage stage,
    unsigned int * outShader) noexcept
{
    if (!outShader)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outShader = 0;
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    bool unsupp = false;
    const GLenum glStage = GpuPipelineStageToGLenum(stage, &unsupp);
    if (unsupp || glStage == 0)
        return static_cast<int>(GpuShaderCompileError::STAGE_UNSUPPORTED);
    const GLuint sh = glCreateShader(glStage);
    if (!sh)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outShader = static_cast<unsigned int>(sh);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderSetShaderSource(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    const char * source) noexcept
{
    if (!source || shader == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    glShaderSource(static_cast<GLuint>(shader), 1, &source, nullptr);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderCompileShaderObject(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    char * infoLog,
    std::size_t infoLogCap) noexcept
{
    ClearLog(infoLog, infoLogCap);
    if (shader == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    glCompileShader(static_cast<GLuint>(shader));
    GLint ok = 0;
    glGetShaderiv(static_cast<GLuint>(shader), GL_COMPILE_STATUS, &ok);
    if (!ok) {
        FetchShaderLog(static_cast<GLuint>(shader), infoLog, infoLogCap);
        SetInternalApiError("GpuShaderCompileShaderObject: compile failed.");
        return static_cast<int>(GpuShaderCompileError::SHADER_COMPILE_FAILED);
    }
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderCreateProgramObject(ApplicationContextSettings const * ctx, unsigned int * outProgram) noexcept
{
    if (!outProgram)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outProgram = 0;
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    const GLuint p = glCreateProgram();
    if (!p)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outProgram = static_cast<unsigned int>(p);
    return static_cast<int>(GpuShaderCompileError::OK);
}
void GpuShaderAttachShader(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int shader) noexcept
{
    if (program == 0 || shader == 0 || !BackendAllowsGlsl(ctx))
        return;
    glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
}
int GpuShaderBindAttribLocation(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int location,
    const char * name) noexcept
{
    if (program == 0 || !name)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    glBindAttribLocation(static_cast<GLuint>(program), static_cast<GLuint>(location), name);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderLinkProgramObject(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    char * infoLog,
    std::size_t infoLogCap) noexcept
{
    ClearLog(infoLog, infoLogCap);
    if (program == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    glLinkProgram(static_cast<GLuint>(program));
    GLint lok = 0;
    glGetProgramiv(static_cast<GLuint>(program), GL_LINK_STATUS, &lok);
    if (!lok) {
        FetchProgramLog(static_cast<GLuint>(program), infoLog, infoLogCap);
        SetInternalApiError("GpuShaderLinkProgramObject: link failed.");
        return static_cast<int>(GpuShaderCompileError::LINK_FAILED);
    }
    return static_cast<int>(GpuShaderCompileError::OK);
}
void GpuShaderDeleteShaderObject(unsigned int shader) noexcept
{
    if (shader == 0)
        return;
    glDeleteShader(static_cast<GLuint>(shader));
}

#else

int GpuShaderCreateObject(
    ApplicationContextSettings const * ctx,
    GpuShaderPipelineStage stage,
    unsigned int * outShader) noexcept
{
    if (!outShader)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outShader = 0;
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs())
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bool unsupp = false;
    const GLenum glStage = GpuPipelineStageToGLenum(stage, &unsupp);
    if (unsupp || glStage == 0)
        return static_cast<int>(GpuShaderCompileError::STAGE_UNSUPPORTED);
    const GLuint sh = s_glCreateShader(glStage);
    if (!sh)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outShader = static_cast<unsigned int>(sh);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderSetShaderSource(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    const char * source) noexcept
{
    if (!source || shader == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs())
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    s_glShaderSource(static_cast<GLuint>(shader), 1, &source, nullptr);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderCompileShaderObject(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    char * infoLog,
    std::size_t infoLogCap) noexcept
{
    ClearLog(infoLog, infoLogCap);
    if (shader == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs())
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    s_glCompileShader(static_cast<GLuint>(shader));
    GLint ok = 0;
    s_glGetShaderiv(static_cast<GLuint>(shader), GL_COMPILE_STATUS, &ok);
    if (!ok) {
        FetchShaderLog(static_cast<GLuint>(shader), infoLog, infoLogCap);
        SetInternalApiError("GpuShaderCompileShaderObject: compile failed.");
        return static_cast<int>(GpuShaderCompileError::SHADER_COMPILE_FAILED);
    }
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderCreateProgramObject(ApplicationContextSettings const * ctx, unsigned int * outProgram) noexcept
{
    if (!outProgram)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outProgram = 0;
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs())
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    const GLuint p = s_glCreateProgram();
    if (!p)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outProgram = static_cast<unsigned int>(p);
    return static_cast<int>(GpuShaderCompileError::OK);
}
void GpuShaderAttachShader(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int shader) noexcept
{
    if (program == 0 || shader == 0 || !BackendAllowsGlsl(ctx))
        return;
    if (!LoadShaderProcs())
        return;
    s_glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
}
int GpuShaderBindAttribLocation(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int location,
    const char * name) noexcept
{
    if (program == 0 || !name)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs() || !s_glBindAttribLocation)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    s_glBindAttribLocation(static_cast<GLuint>(program), static_cast<GLuint>(location), name);
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuShaderLinkProgramObject(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    char * infoLog,
    std::size_t infoLogCap) noexcept
{
    ClearLog(infoLog, infoLogCap);
    if (program == 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!BackendAllowsGlsl(ctx))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (!LoadShaderProcs())
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    s_glLinkProgram(static_cast<GLuint>(program));
    GLint lok = 0;
    s_glGetProgramiv(static_cast<GLuint>(program), GL_LINK_STATUS, &lok);
    if (!lok) {
        FetchProgramLog(static_cast<GLuint>(program), infoLog, infoLogCap);
        SetInternalApiError("GpuShaderLinkProgramObject: link failed.");
        return static_cast<int>(GpuShaderCompileError::LINK_FAILED);
    }
    return static_cast<int>(GpuShaderCompileError::OK);
}
void GpuShaderDeleteShaderObject(unsigned int shader) noexcept
{
    if (shader == 0)
        return;
    if (LoadShaderProcs())
        s_glDeleteShader(static_cast<GLuint>(shader));
}

#endif

void GpuShaderDeleteProgram(unsigned int program) noexcept
{
    if (program == 0)
        return;
#if HONKORDGL_PLATFORM_ANDROID
    glDeleteProgram(static_cast<GLuint>(program));
#elif HONKORDGL_PLATFORM_APPLE
    glDeleteProgram(static_cast<GLuint>(program));
#else
    if (LoadShaderProcs())
        s_glDeleteProgram(static_cast<GLuint>(program));
#endif
}

} // namespace HonkordGL::Graphics