/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Minimal deferred renderer loop: geometry pass → G-buffer (MRT) → fullscreen lighting → swapchain.
 * Internal GPU implementation for the deferred renderer sample.
 * Public entrypoint lives in `src/DeferredRendererSample.cpp` and delegates here.
 *
 * Example build (Linux + X11/Wayland, from repo root — link all three files):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     tests/DeferredRenderer/TestDeferredRenderer.cpp \
 *     src/DeferredRendererSample.cpp src/internal/DeferredRendererGpuApi.cpp \
 *     src/GpuShaderCompiler.cpp \
 *     src/Video.cpp src/WindowBackend.cpp \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/X11/GLXRendererContext.cpp src/X11/EGLRendererContextX11.cpp \
 *     src/Wayland/EGLRendererContextWayland.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/GpuRenderer.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o tests/DeferredRenderer/TestDeferredRenderer \
 *     -lX11 -lXrandr -lXi -lwayland-client -lwayland-egl -lEGL -lGL
 *
 *   cd tests/DeferredRenderer && ./TestDeferredRenderer
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/GpuRenderer.h>

#include "DeferredRendererGpuApi.h"
#include "DesktopGlIncludes.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr char kGeomVs[] = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
uniform mat4 u_mvp;
uniform mat4 u_mv;
uniform mat3 u_normal_mv;
out vec3 v_nrm_view;
out vec3 v_pos_view;
void main() {
  vec4 pos_v = u_mv * vec4(a_pos, 1.0);
  v_pos_view = pos_v.xyz;
  v_nrm_view = u_normal_mv * a_nrm;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

constexpr char kGeomFs[] = R"(#version 330 core
in vec3 v_nrm_view;
in vec3 v_pos_view;
uniform vec3 u_albedo;
layout(location = 0) out vec4 g_albedo;
layout(location = 1) out vec4 g_normal;
layout(location = 2) out vec4 g_position;
void main() {
  g_albedo = vec4(u_albedo, 1.0);
  g_normal = vec4(normalize(v_nrm_view), 0.0);
  g_position = vec4(v_pos_view, 1.0);
}
)";

constexpr char kLightVs[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
  v_uv = a_pos * 0.5 + 0.5;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

constexpr char kLightFs[] = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_albedo;
uniform sampler2D u_normal;
uniform sampler2D u_position;
uniform sampler2D u_depth;
uniform vec3 u_light_dir_view;
uniform vec3 u_light_color;
out vec4 o_color;
void main() {
  vec4 alb = texture(u_albedo, v_uv);
  vec3 n = normalize(texture(u_normal, v_uv).xyz);
  float d = texture(u_depth, v_uv).r;
  if (d >= 0.99999) {
    o_color = vec4(0.02, 0.03, 0.05, 1.0);
    return;
  }
  vec3 L = normalize(-u_light_dir_view);
  float ndl = max(dot(n, L), 0.0);
  vec3 diffuse = u_light_color * ndl * alb.rgb;
  vec3 ambient = 0.08 * alb.rgb;
  o_color = vec4(ambient + diffuse, 1.0);
}
)";

// Unit cube [-0.5,0.5]^3 — 24 verts (face normals), 36 indices
static const float kCubePosNor[] = {
    // +Z
    -0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 0, 1, -0.5f, 0.5f, 0.5f, 0, 0, 1,
    // -Z
    0.5f, -0.5f, -0.5f, 0, 0, -1, -0.5f, -0.5f, -0.5f, 0, 0, -1, -0.5f, 0.5f, -0.5f, 0, 0, -1, 0.5f, 0.5f, -0.5f, 0, 0, -1,
    // +X
    0.5f, -0.5f, 0.5f, 1, 0, 0, 0.5f, -0.5f, -0.5f, 1, 0, 0, 0.5f, 0.5f, -0.5f, 1, 0, 0, 0.5f, 0.5f, 0.5f, 1, 0, 0,
    // -X
    -0.5f, -0.5f, -0.5f, -1, 0, 0, -0.5f, -0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, -0.5f, -1, 0, 0,
    // +Y
    -0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, -0.5f, 0, 1, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0,
    // -Y
    -0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, 0.5f, 0, -1, 0, -0.5f, -0.5f, 0.5f, 0, -1, 0,
};
static const unsigned short kCubeIdx[] = {
    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
};

static const float kFsTriPos[] = {
    -1.f, -1.f,
    3.f, -1.f,
    -1.f, 3.f,
};

void Mat4Identity(float * m)
{
    std::memset(m, 0, 16u * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.f;
}
void Mat4Mul(float * out, const float * a, const float * b)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2]
                + a[3 * 4 + r] * b[c * 4 + 3];
}
void Mat4Perspective(float * m, float fovyDeg, float aspect, float zNear, float zFar)
{
    std::memset(m, 0, 16u * sizeof(float));
    const float rad = fovyDeg * (3.14159265358979323846f / 180.f);
    const float f = 1.f / std::tan(rad * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.f;
    m[14] = (2.f * zFar * zNear) / (zNear - zFar);
}
void Mat4Lookat(float * m, float ex, float ey, float ez, float cx, float cy, float cz, float ux, float uy, float uz)
{
    float fx = cx - ex;
    float fy = cy - ey;
    float fz = cz - ez;
    const float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 1e-6f) {
        fx /= flen;
        fy /= flen;
        fz /= flen;
    }
    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    const float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (slen > 1e-6f) {
        sx /= slen;
        sy /= slen;
        sz /= slen;
    }
    const float uxp = sy * fz - sz * fy;
    const float uyp = sz * fx - sx * fz;
    const float uzp = sx * fy - sy * fx;

    m[0] = sx;
    m[1] = uxp;
    m[2] = -fx;
    m[3] = 0.f;
    m[4] = sy;
    m[5] = uyp;
    m[6] = -fy;
    m[7] = 0.f;
    m[8] = sz;
    m[9] = uzp;
    m[10] = -fz;
    m[11] = 0.f;
    m[12] = -(sx * ex + sy * ey + sz * ez);
    m[13] = -(uxp * ex + uyp * ey + uzp * ez);
    m[14] = (fx * ex + fy * ey + fz * ez);
    m[15] = 1.f;
}
void Mat4Translate(float * m, float tx, float ty, float tz)
{
    Mat4Identity(m);
    m[12] = tx;
    m[13] = ty;
    m[14] = tz;
}
void Mat4RotateY(float * m, float rad)
{
    Mat4Identity(m);
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    m[0] = c;
    m[8] = s;
    m[2] = -s;
    m[10] = c;
}
void Mat4Scale(float * m, float sx, float sy, float sz)
{
    Mat4Identity(m);
    m[0] = sx;
    m[5] = sy;
    m[10] = sz;
}
void Mat3FromMat4Mv(float * out3, const float * mv4)
{
    out3[0] = mv4[0];
    out3[1] = mv4[1];
    out3[2] = mv4[2];
    out3[3] = mv4[4];
    out3[4] = mv4[5];
    out3[5] = mv4[6];
    out3[6] = mv4[8];
    out3[7] = mv4[9];
    out3[8] = mv4[10];
}

#if !HONKORDGL_PLATFORM_APPLE

using PFN_glGenVertexArrays = void (*)(GLsizei, GLuint *);
using PFN_glDeleteVertexArrays = void (*)(GLsizei, const GLuint *);
using PFN_glBindVertexArray = void (*)(GLuint);
using PFN_glGenBuffers = void (*)(GLsizei, GLuint *);
using PFN_glDeleteBuffers = void (*)(GLsizei, const GLuint *);
using PFN_glBindBuffer = void (*)(GLenum, GLuint);
using PFN_glBufferData = void (*)(GLenum, GLsizeiptr, const void *, GLenum);
using PFN_glVertexAttribPointer = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
using PFN_glEnableVertexAttribArray = void (*)(GLuint);
using PFN_glDisableVertexAttribArray = void (*)(GLuint);
using PFN_glCreateShader = GLuint (*)(GLenum);
using PFN_glShaderSource = void (*)(GLuint, GLsizei, const GLchar * const *, const GLint *);
using PFN_glCompileShader = void (*)(GLuint);
using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteShader = void (*)(GLuint);
using PFN_glCreateProgram = GLuint (*)(void);
using PFN_glAttachShader = void (*)(GLuint, GLuint);
using PFN_glLinkProgram = void (*)(GLuint);
using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint *);
using PFN_glGetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei *, GLchar *);
using PFN_glDeleteProgram = void (*)(GLuint);
using PFN_glUseProgram = void (*)(GLuint);
using PFN_glUniformMatrix4fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniformMatrix3fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniform3fv = void (*)(GLint, GLsizei, const GLfloat *);
using PFN_glUniform2fv = void (*)(GLint, GLsizei, const GLfloat *);
using PFN_glUniform1i = void (*)(GLint, GLint);
using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar *);
using PFN_glGenFramebuffers = void (*)(GLsizei, GLuint *);
using PFN_glDeleteFramebuffers = void (*)(GLsizei, const GLuint *);
using PFN_glBindFramebuffer = void (*)(GLenum, GLuint);
using PFN_glCheckFramebufferStatus = GLenum (*)(GLenum);
using PFN_glFramebufferTexture2D = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFN_glDrawBuffers = void (*)(GLsizei, const GLenum *);
using PFN_glActiveTexture = void (*)(GLenum);

#define DEF_PROC(name) static PFN_##name s_##name{}

DEF_PROC(glGenVertexArrays);
DEF_PROC(glDeleteVertexArrays);
DEF_PROC(glBindVertexArray);
DEF_PROC(glGenBuffers);
DEF_PROC(glDeleteBuffers);
DEF_PROC(glBindBuffer);
DEF_PROC(glBufferData);
DEF_PROC(glVertexAttribPointer);
DEF_PROC(glEnableVertexAttribArray);
DEF_PROC(glDisableVertexAttribArray);
DEF_PROC(glCreateShader);
DEF_PROC(glShaderSource);
DEF_PROC(glCompileShader);
DEF_PROC(glGetShaderiv);
DEF_PROC(glGetShaderInfoLog);
DEF_PROC(glDeleteShader);
DEF_PROC(glCreateProgram);
DEF_PROC(glAttachShader);
DEF_PROC(glLinkProgram);
DEF_PROC(glGetProgramiv);
DEF_PROC(glGetProgramInfoLog);
DEF_PROC(glDeleteProgram);
DEF_PROC(glUseProgram);
DEF_PROC(glUniformMatrix4fv);
DEF_PROC(glUniformMatrix3fv);
DEF_PROC(glUniform3fv);
DEF_PROC(glUniform2fv);
DEF_PROC(glUniform1i);
DEF_PROC(glGetUniformLocation);
DEF_PROC(glGenFramebuffers);
DEF_PROC(glDeleteFramebuffers);
DEF_PROC(glBindFramebuffer);
DEF_PROC(glCheckFramebufferStatus);
DEF_PROC(glFramebufferTexture2D);
DEF_PROC(glDrawBuffers);
DEF_PROC(glActiveTexture);

#undef DEF_PROC

/** Set after `MakeCurrent` so `LoadProc` uses `GpuRenderer::GetGraphicsProc`. */
static GpuRenderer * gDeferredGpuForProcs = nullptr;

static void * LoadProc(const char * name) noexcept
{
    if (gDeferredGpuForProcs)
        return gDeferredGpuForProcs->GetGraphicsProc(name);
#if HONKORDGL_PLATFORM_WINDOWS
    void * p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p)
        return p;
    return reinterpret_cast<void *>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));
#else
    return reinterpret_cast<void *>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#endif
}
static bool LoadAllProcs() noexcept
{
    if (s_glGenFramebuffers)
        return true;
#define LOAD(n) s_##n = reinterpret_cast<PFN_##n>(LoadProc(#n))
    LOAD(glGenVertexArrays);
    LOAD(glDeleteVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glGenBuffers);
    LOAD(glDeleteBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glVertexAttribPointer);
    LOAD(glEnableVertexAttribArray);
    LOAD(glDisableVertexAttribArray);
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glDeleteShader);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glDeleteProgram);
    LOAD(glUseProgram);
    LOAD(glUniformMatrix4fv);
    LOAD(glUniformMatrix3fv);
    LOAD(glUniform3fv);
    LOAD(glUniform2fv);
    LOAD(glUniform1i);
    LOAD(glGetUniformLocation);
    LOAD(glGenFramebuffers);
    LOAD(glDeleteFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glCheckFramebufferStatus);
    LOAD(glFramebufferTexture2D);
    LOAD(glDrawBuffers);
    LOAD(glActiveTexture);
#undef LOAD
    return s_glGenFramebuffers && s_glBindFramebuffer && s_glFramebufferTexture2D && s_glDrawBuffers && s_glGenVertexArrays
        && s_glCreateProgram && s_glLinkProgram && s_glUniform1i;
}
static GLuint CompileShader(GLenum type, const char * src)
{
    const GLuint s = s_glCreateShader(type);
    s_glShaderSource(s, 1, &src, nullptr);
    s_glCompileShader(s);
    GLint ok = 0;
    s_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        s_glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint LinkProgram(GLuint vs, GLuint fs)
{
    const GLuint p = s_glCreateProgram();
    s_glAttachShader(p, vs);
    s_glAttachShader(p, fs);
    s_glLinkProgram(p);
    GLint ok = 0;
    s_glGetProgramiv(p, GL_LINK_STATUS, &ok);
    s_glDeleteShader(vs);
    s_glDeleteShader(fs);
    if (!ok) {
        s_glDeleteProgram(p);
        return 0;
    }
    return p;
}

#define HOGL(name) s_##name

#else

static GLuint CompileShader(GLenum type, const char * src)
{
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint LinkProgram(GLuint vs, GLuint fs)
{
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

#define HOGL(name) name

#endif

struct GBuffer
{
    int w{0};
    int h{0};
    GLuint fbo{0};
    GLuint albedo{0};
    GLuint normal{0};
    GLuint position{0};
    GLuint depth{0};
};
void DestroyGBuffer(GBuffer & g)
{
    if (g.fbo) {
        HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);
        HOGL(glDeleteFramebuffers)(1, &g.fbo);
    }
    if (g.albedo)
        glDeleteTextures(1, &g.albedo);
    if (g.normal)
        glDeleteTextures(1, &g.normal);
    if (g.position)
        glDeleteTextures(1, &g.position);
    if (g.depth)
        glDeleteTextures(1, &g.depth);
    g = GBuffer{};
}
bool CreateGBuffer(GBuffer & g, int w, int h)
{
    DestroyGBuffer(g);
    if (w <= 0 || h <= 0)
        return false;
    g.w = w;
    g.h = h;

    HOGL(glGenFramebuffers)(1, &g.fbo);
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, g.fbo);

    auto MakeTex = [&](GLuint * tex, GLenum internal, GLenum format, GLenum type, GLenum attach)
    {
        glGenTextures(1, tex);
        glBindTexture(GL_TEXTURE_2D, *tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal), w, h, 0, format, type, nullptr);
        HOGL(glFramebufferTexture2D)(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, *tex, 0);
    };

    MakeTex(&g.albedo, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0);
    MakeTex(&g.normal, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_COLOR_ATTACHMENT1);
    MakeTex(&g.position, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_COLOR_ATTACHMENT2);

    glGenTextures(1, &g.depth);
    glBindTexture(GL_TEXTURE_2D, g.depth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    HOGL(glFramebufferTexture2D)(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g.depth, 0);

    const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    HOGL(glDrawBuffers)(3, bufs);

    const GLenum st = HOGL(glCheckFramebufferStatus)(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        DestroyGBuffer(g);
        return false;
    }
    return true;
}
struct DeferredSampleGpuState
{
    std::unique_ptr<GpuRenderer> gpu;
    ApplicationContextSettings * app = nullptr;
    WindowBackend * backend = nullptr;
    GBuffer gb{};
    int fb_w = 0;
    int fb_h = 0;
    GLuint prog_geom = 0;
    GLuint prog_light = 0;
    GLuint vao_cube = 0;
    GLuint vbo_cube = 0;
    GLuint ebo_cube = 0;
    GLuint vao_fs = 0;
    GLuint vbo_fs = 0;
    GLint loc_mvp = 0;
    GLint loc_mv = 0;
    GLint loc_nm = 0;
    GLint loc_alb_g = 0;
    GLint loc_alb = 0;
    GLint loc_nor = 0;
    GLint loc_pos = 0;
    GLint loc_dep = 0;
    GLint loc_ldir = 0;
    GLint loc_lcol = 0;
    std::chrono::steady_clock::time_point t0{};
};
static bool InitDeferredSampleGpuState(
    DeferredSampleGpuState & s,
    ApplicationContextSettings & app,
    WindowBackend & backend,
    HonkordGL::Graphics::DeferredRendererSampleSettings const & settings) noexcept
{
    s.app = &app;
    s.backend = &backend;
    s.gpu = std::make_unique<GpuRenderer>(app);
    if (s.gpu->CreateBestAvailableShaderBackend() != 0) {
        std::cerr << "GpuRenderer::CreateBestAvailableShaderBackend failed.\n";
        s.gpu.reset();
        return false;
    }

    s.gpu->MakeCurrent();
    s.gpu->SetDefaultViewport();

#if !HONKORDGL_PLATFORM_APPLE
    gDeferredGpuForProcs = s.gpu.get();
    (void)s.gpu->LoadShaderCompilerProcs();
    if (!LoadAllProcs()) {
        std::cerr << "DeferredRenderer: failed to load GL 3.x entry points.\n";
        gDeferredGpuForProcs = nullptr;
        s.gpu.reset();
        return false;
    }
#endif

    const GLuint vsG = CompileShader(GL_VERTEX_SHADER, kGeomVs);
    const GLuint fsG = CompileShader(GL_FRAGMENT_SHADER, kGeomFs);
    const GLuint vsL = CompileShader(GL_VERTEX_SHADER, kLightVs);
    const GLuint fsL = CompileShader(GL_FRAGMENT_SHADER, kLightFs);
    s.prog_geom = (vsG && fsG) ? LinkProgram(vsG, fsG) : 0;
    s.prog_light = (vsL && fsL) ? LinkProgram(vsL, fsL) : 0;
    if (!s.prog_geom || !s.prog_light) {
        std::cerr << "Shader compile/link failed.\n";
        if (s.prog_geom)
            s.gpu->DeleteShaderProgram(s.prog_geom);
        if (s.prog_light)
            s.gpu->DeleteShaderProgram(s.prog_light);
        s.prog_geom = s.prog_light = 0;
        gDeferredGpuForProcs = nullptr;
        s.gpu.reset();
        return false;
    }

    HOGL(glGenVertexArrays)(1, &s.vao_cube);
    HOGL(glGenBuffers)(1, &s.vbo_cube);
    HOGL(glGenBuffers)(1, &s.ebo_cube);
    HOGL(glBindVertexArray)(s.vao_cube);
    HOGL(glBindBuffer)(GL_ARRAY_BUFFER, s.vbo_cube);
    HOGL(glBufferData)(GL_ARRAY_BUFFER, sizeof(kCubePosNor), kCubePosNor, GL_STATIC_DRAW);
    HOGL(glBindBuffer)(GL_ELEMENT_ARRAY_BUFFER, s.ebo_cube);
    HOGL(glBufferData)(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIdx), kCubeIdx, GL_STATIC_DRAW);
    HOGL(glEnableVertexAttribArray)(0);
    HOGL(glVertexAttribPointer)(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(0));
    HOGL(glEnableVertexAttribArray)(1);
    HOGL(glVertexAttribPointer)(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));

    HOGL(glGenVertexArrays)(1, &s.vao_fs);
    HOGL(glGenBuffers)(1, &s.vbo_fs);
    HOGL(glBindVertexArray)(s.vao_fs);
    HOGL(glBindBuffer)(GL_ARRAY_BUFFER, s.vbo_fs);
    HOGL(glBufferData)(GL_ARRAY_BUFFER, sizeof(kFsTriPos), kFsTriPos, GL_STATIC_DRAW);
    HOGL(glEnableVertexAttribArray)(0);
    HOGL(glVertexAttribPointer)(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    HOGL(glBindVertexArray)(0);

    s.fb_w = app.frame_buffer_width > 0 ? app.frame_buffer_width : app.client_rect.w;
    s.fb_h = app.frame_buffer_height > 0 ? app.frame_buffer_height : app.client_rect.z;
    if (!CreateGBuffer(s.gb, s.fb_w, s.fb_h)) {
        std::cerr << "G-buffer FBO incomplete.\n";
        gDeferredGpuForProcs = nullptr;
        HOGL(glDeleteBuffers)(1, &s.vbo_cube);
        HOGL(glDeleteBuffers)(1, &s.ebo_cube);
        HOGL(glDeleteVertexArrays)(1, &s.vao_cube);
        HOGL(glDeleteBuffers)(1, &s.vbo_fs);
        HOGL(glDeleteVertexArrays)(1, &s.vao_fs);
        s.vao_cube = s.vbo_cube = s.ebo_cube = s.vao_fs = s.vbo_fs = 0;
        s.gpu->DeleteShaderProgram(s.prog_geom);
        s.gpu->DeleteShaderProgram(s.prog_light);
        s.prog_geom = s.prog_light = 0;
        s.gpu.reset();
        return false;
    }

    s.loc_mvp = HOGL(glGetUniformLocation)(s.prog_geom, "u_mvp");
    s.loc_mv = HOGL(glGetUniformLocation)(s.prog_geom, "u_mv");
    s.loc_nm = HOGL(glGetUniformLocation)(s.prog_geom, "u_normal_mv");
    s.loc_alb_g = HOGL(glGetUniformLocation)(s.prog_geom, "u_albedo");

    s.loc_alb = HOGL(glGetUniformLocation)(s.prog_light, "u_albedo");
    s.loc_nor = HOGL(glGetUniformLocation)(s.prog_light, "u_normal");
    s.loc_pos = HOGL(glGetUniformLocation)(s.prog_light, "u_position");
    s.loc_dep = HOGL(glGetUniformLocation)(s.prog_light, "u_depth");
    s.loc_ldir = HOGL(glGetUniformLocation)(s.prog_light, "u_light_dir_view");
    s.loc_lcol = HOGL(glGetUniformLocation)(s.prog_light, "u_light_color");

    backend.SetTargetFrameRate(settings.target_fps != 0 ? settings.target_fps : 60);
    s.t0 = std::chrono::steady_clock::now();
    return true;
}
static void DestroyDeferredSampleGpuState(DeferredSampleGpuState & s) noexcept
{
    DestroyGBuffer(s.gb);
    if (s.gpu) {
        if (s.prog_geom)
            s.gpu->DeleteShaderProgram(s.prog_geom);
        if (s.prog_light)
            s.gpu->DeleteShaderProgram(s.prog_light);
    }
    s.prog_geom = s.prog_light = 0;
    gDeferredGpuForProcs = nullptr;
    if (s.vbo_cube)
        HOGL(glDeleteBuffers)(1, &s.vbo_cube);
    if (s.ebo_cube)
        HOGL(glDeleteBuffers)(1, &s.ebo_cube);
    if (s.vao_cube)
        HOGL(glDeleteVertexArrays)(1, &s.vao_cube);
    if (s.vbo_fs)
        HOGL(glDeleteBuffers)(1, &s.vbo_fs);
    if (s.vao_fs)
        HOGL(glDeleteVertexArrays)(1, &s.vao_fs);
    s.vao_cube = s.vbo_cube = s.ebo_cube = s.vao_fs = s.vbo_fs = 0;
    if (s.gpu) {
        s.gpu->Destroy();
        s.gpu.reset();
    }
}
static void RunDeferredSampleGpuFrame(
    DeferredSampleGpuState & s,
    HonkordGL::Graphics::DeferredRendererSampleSettings const & settings) noexcept
{
    GpuRenderer & gpu = *s.gpu;
    GBuffer & gb = s.gb;
    int const fbw = s.fb_w;
    int const fbh = s.fb_h;

    gpu.MakeCurrent();
    gpu.SetDefaultViewport();

    if (s.app) {
        const int aw = s.app->frame_buffer_width > 0 ? s.app->frame_buffer_width : s.app->client_rect.w;
        const int ah = s.app->frame_buffer_height > 0 ? s.app->frame_buffer_height : s.app->client_rect.z;
        if (aw > 0 && ah > 0 && (aw != s.fb_w || ah != s.fb_h)) {
            s.fb_w = aw;
            s.fb_h = ah;
            if (!CreateGBuffer(s.gb, s.fb_w, s.fb_h))
                return;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const float tsec = std::chrono::duration<float>(t1 - s.t0).count();

    float proj[16];
    float view[16];
    float model[16];
    float mv[16];
    float mvp[16];
    float nmat[9];

    const float aspect = fbw > 0 && fbh > 0 ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.f;
    Mat4Perspective(proj, 55.f, aspect, 0.1f, 64.f);
    Mat4Lookat(view, 2.2f, 1.4f, 2.8f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
    Mat4Identity(model);
    {
        float tr[16], ry[16], tmp[16];
        Mat4Translate(tr, 0.f, 0.35f, 0.f);
        Mat4RotateY(ry, tsec * 0.7f);
        Mat4Mul(tmp, ry, tr);
        float sc[16];
        Mat4Scale(sc, 1.f, 1.f, 1.f);
        Mat4Mul(model, tmp, sc);
    }
    Mat4Mul(mv, view, model);
    Mat4Mul(mvp, proj, mv);
    Mat3FromMat4Mv(nmat, mv);

    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, gb.fbo);
    gpu.SetViewport(0, 0, fbw, fbh);
    const GLenum bufs2[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    HOGL(glDrawBuffers)(3, bufs2);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gpu.BindShaderProgram(s.prog_geom);
    HOGL(glUniformMatrix4fv)(s.loc_mvp, 1, GL_FALSE, mvp);
    HOGL(glUniformMatrix4fv)(s.loc_mv, 1, GL_FALSE, mv);
    HOGL(glUniformMatrix3fv)(s.loc_nm, 1, GL_FALSE, nmat);
    HOGL(glUniform3fv)(s.loc_alb_g, 1, settings.mesh_albedo);
    HOGL(glBindVertexArray)(s.vao_cube);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);

    HOGL(glBindFramebuffer)(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    gpu.SetViewport(0, 0, fbw, fbh);
    glDisable(GL_DEPTH_TEST);
    glClearColor(
        settings.background_rgb[0],
        settings.background_rgb[1],
        settings.background_rgb[2],
        1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    gpu.BindShaderProgram(s.prog_light);
    HOGL(glActiveTexture)(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gb.albedo);
    HOGL(glUniform1i)(s.loc_alb, 0);
    HOGL(glActiveTexture)(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gb.normal);
    HOGL(glUniform1i)(s.loc_nor, 1);
    HOGL(glActiveTexture)(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gb.position);
    HOGL(glUniform1i)(s.loc_pos, 2);
    HOGL(glActiveTexture)(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gb.depth);
    HOGL(glUniform1i)(s.loc_dep, 3);

    float lightDirWorld[] = {
        settings.light_dir_world[0],
        settings.light_dir_world[1],
        settings.light_dir_world[2],
    };
    const float lwlen = std::sqrt(lightDirWorld[0] * lightDirWorld[0] + lightDirWorld[1] * lightDirWorld[1]
            + lightDirWorld[2] * lightDirWorld[2]);
    if (lwlen > 1e-6f) {
        lightDirWorld[0] /= lwlen;
        lightDirWorld[1] /= lwlen;
        lightDirWorld[2] /= lwlen;
    }
    float lightDirView[3] = {
        view[0] * lightDirWorld[0] + view[4] * lightDirWorld[1] + view[8] * lightDirWorld[2],
        view[1] * lightDirWorld[0] + view[5] * lightDirWorld[1] + view[9] * lightDirWorld[2],
        view[2] * lightDirWorld[0] + view[6] * lightDirWorld[1] + view[10] * lightDirWorld[2],
    };
    HOGL(glUniform3fv)(s.loc_ldir, 1, lightDirView);
    HOGL(glUniform3fv)(s.loc_lcol, 1, settings.light_color);

    HOGL(glBindVertexArray)(s.vao_fs);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindTexture(GL_TEXTURE_2D, 0);
    gpu.BindShaderProgram(0);
    HOGL(glBindVertexArray)(0);

    gpu.SwapBuffers();
    if (s.backend)
        s.backend->DelayFrame();
}

} // namespace

namespace HonkordGL::Graphics
{
struct DeferredRendererSampleContext
{
    void * opaque = nullptr;
};

} // namespace HonkordGL::Graphics

namespace
{
using HonkordGL::Graphics::DeferredRendererSampleContext;
using HonkordGL::Graphics::DeferredRendererSampleSettings;

static int CreateDeferredSampleWrapped(
    ApplicationContextSettings & app,
    WindowBackend & backend,
    DeferredRendererSampleSettings const & settings,
    DeferredRendererSampleContext * * outContext) noexcept
{
    auto * wrap = new DeferredRendererSampleContext();
    auto * st = new DeferredSampleGpuState();
    if (!InitDeferredSampleGpuState(*st, app, backend, settings)) {
        delete st;
        delete wrap;
        return 1;
    }
    wrap->opaque = st;
    *outContext = wrap;
    return 0;
}
static void RunDeferredSampleWrapped(
    DeferredRendererSampleContext * context,
    DeferredRendererSampleSettings const & settings) noexcept
{
    if (!context || !context->opaque)
        return;
    RunDeferredSampleGpuFrame(*static_cast<DeferredSampleGpuState *>(context->opaque), settings);
}
static void DestroyDeferredSampleWrapped(DeferredRendererSampleContext * context) noexcept
{
    if (!context)
        return;
    if (context->opaque) {
        DestroyDeferredSampleGpuState(*static_cast<DeferredSampleGpuState *>(context->opaque));
        delete static_cast<DeferredSampleGpuState *>(context->opaque);
        context->opaque = nullptr;
    }
    delete context;
}

} // namespace

namespace HonkordGL::Graphics::Internal
{

int CreateDeferredRendererGpuApi(
    ApplicationContextSettings & app,
    WindowBackend & backend,
    DeferredRendererSampleSettings const & settings,
    DeferredRendererSampleContext * * outContext) noexcept
{
    return CreateDeferredSampleWrapped(app, backend, settings, outContext);
}
void RunDeferredRendererGpuApiFrame(
    DeferredRendererSampleContext * context,
    DeferredRendererSampleSettings const & settings) noexcept
{
    RunDeferredSampleWrapped(context, settings);
}
void DestroyDeferredRendererGpuApi(DeferredRendererSampleContext * context) noexcept
{
    DestroyDeferredSampleWrapped(context);
}

} // namespace HonkordGL::Graphics::Internal