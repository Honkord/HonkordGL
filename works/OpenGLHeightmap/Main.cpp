/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Procedural heightmap mesh (indexed triangles) with OpenGL 3.3, `GpuRenderer`, and
 * `OpenGlIntegrationQueryHints` after attach. Orbit camera (auto-rotate); arrow keys adjust orbit,
 * PageUp/PageDown zoom.
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/OpenGlIntegration.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

#include "internal/DesktopGlIncludes.h"

#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateWindowW
#undef CreateWindowW
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::GpuBufferTarget;
using HonkordGL::Graphics::GpuBufferUsage;
using HonkordGL::Graphics::GpuIndexType;
using HonkordGL::Graphics::GpuObjectName;
using HonkordGL::Graphics::GpuPrimitive;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::GpuShaderCompileError;
using HonkordGL::Graphics::OpenGlIntegrationHints;
using HonkordGL::Graphics::OpenGlIntegrationQueryHints;
using HonkordGL::Graphics::OpenGlIntegrationResult;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr int kGridN = 96; // (kGridN+1)^2 vertices

float HeightAt(float x, float z) noexcept
{
    return 2.2f * std::sin(x * 0.08f) * std::cos(z * 0.07f) + 1.1f * std::sin(x * 0.15f + z * 0.11f)
        + 0.45f * std::sin(x * 0.35f) * std::sin(z * 0.28f);
}

void HeightGradient(float x, float z, float & outDu, float & outDv) noexcept
{
    constexpr float e = 0.35f;
    const float h0 = HeightAt(x, z);
    outDu = (HeightAt(x + e, z) - h0) / e;
    outDv = (HeightAt(x, z + e) - h0) / e;
}

void NormalAt(float x, float z, float * n3) noexcept
{
    float du = 0.f;
    float dv = 0.f;
    HeightGradient(x, z, du, dv);
    // tangents: (1, du, 0) and (0, dv, 1); cross(tu, tv) = (du, -1, dv), flip toward +Y
    float nx = -du;
    float ny = 1.f;
    float nz = -dv;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = 0.f;
        ny = 1.f;
        nz = 0.f;
    }
    n3[0] = nx;
    n3[1] = ny;
    n3[2] = nz;
}

void Mat4Mul(const float * a, const float * b, float * out) noexcept
{
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2]
                + a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

void Mat4Perspective(float fovyRad, float aspect, float zNear, float zFar, float * m) noexcept
{
    const float f = 1.f / std::tan(fovyRad * 0.5f);
    std::memset(m, 0, 16u * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.f;
    m[14] = (2.f * zFar * zNear) / (zNear - zFar);
}

void Mat4LookAtRh(float ex, float ey, float ez, float ax, float ay, float az, float ux, float uy, float uz,
    float * m) noexcept
{
    float fx = ax - ex;
    float fy = ay - ey;
    float fz = az - ez;
    float rl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (rl > 1e-8f) {
        fx /= rl;
        fy /= rl;
        fz /= rl;
    }
    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;
    rl = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (rl > 1e-8f) {
        sx /= rl;
        sy /= rl;
        sz /= rl;
    }
    float upx = sy * fz - sz * fy;
    float upy = sz * fx - sx * fz;
    float upz = sx * fy - sy * fx;
    // Column-major: three orthonormal axes + separate translation column (matches common GL look-at).
    m[0] = sx;
    m[1] = sy;
    m[2] = sz;
    m[3] = 0.f;
    m[4] = upx;
    m[5] = upy;
    m[6] = upz;
    m[7] = 0.f;
    m[8] = -fx;
    m[9] = -fy;
    m[10] = -fz;
    m[11] = 0.f;
    m[12] = -(sx * ex + sy * ey + sz * ez);
    m[13] = -(upx * ex + upy * ey + upz * ez);
    m[14] = -(-fx * ex - fy * ey - fz * ez);
    m[15] = 1.f;
}

constexpr char kVs[] = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
uniform mat4 u_mvp;
uniform mat3 u_nrm;
out vec3 v_nrm;
out float v_h;
void main() {
  v_nrm = u_nrm * a_nrm;
  v_h = a_pos.y;
  gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

constexpr char kFs[] = R"(#version 330 core
in vec3 v_nrm;
in float v_h;
uniform vec3 u_light;
out vec4 o_color;
void main() {
  vec3 n = normalize(v_nrm);
  float ndl = max(dot(n, normalize(u_light)), 0.0);
  float t = clamp(v_h * 0.12 + 0.35, 0.0, 1.0);
  vec3 lowc = vec3(0.12, 0.35, 0.18);
  vec3 highc = vec3(0.55, 0.52, 0.45);
  vec3 base = mix(lowc, highc, t);
  vec3 amb = 0.14 * base;
  vec3 col = amb + 0.86 * ndl * base;
  o_color = vec4(col, 1.0);
}
)";

using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar *);
using PFN_glUniformMatrix4fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniformMatrix3fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniform3fv = void (*)(GLint, GLsizei, const GLfloat *);
using PFN_glClear = void (*)(GLbitfield);
using PFN_glDepthFunc = void (*)(GLenum);
using PFN_glEnable = void (*)(GLenum);
using PFN_glCullFace = void (*)(GLenum);
using PFN_glFrontFace = void (*)(GLenum);

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL — OpenGLHeightmap";
    settings.client_rect.x = 80;
    settings.client_rect.y = 80;
    settings.client_rect.w = 1280;
    settings.client_rect.z = 720;

    if (!backend.Initialize(nullptr)) {
        std::cerr << "Initialize failed.\n";
        return 1;
    }
    auto window = CreateWindowOnSharedBackend(backend, settings);
    if (!window) {
        std::cerr << "CreateWindowOnSharedBackend failed.\n";
        backend.Shutdown();
        return 1;
    }
    window->CreateWindow();
    ApplicationContextSettings& ctx = window->RetrieveCurrentSettings();
    if (!ctx.window_handle) {
        std::cerr << "CreateWindow failed.\n";
        backend.Shutdown();
        return 1;
    }

    GpuRenderer gpu(ctx);
    if (gpu.CreateBestAvailableShaderBackend() != static_cast<int>(RendererContextResult::OK)) {
        std::cerr << "CreateBestAvailableShaderBackend failed.\n";
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }
    if (gpu.LoadShaderCompilerProcs() != static_cast<int>(GpuShaderCompileError::OK)) {
        std::cerr << "LoadShaderCompilerProcs failed.\n";
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    gpu.MakeCurrent();
    OpenGlIntegrationHints hints{};
    const int qh = OpenGlIntegrationQueryHints(&ctx, &hints);
    if (qh == static_cast<int>(OpenGlIntegrationResult::OK)) {
        std::cout << "OpenGL integration hints: GL " << hints.gl_major_version << '.' << hints.gl_minor_version
            << "  framebuffer " << hints.framebuffer_width << 'x' << hints.framebuffer_height << "  depth "
            << hints.effective_depth_bits << "b\n";
    }

    GpuObjectName program = 0;
    char log[2048]{};
    const int locs[2] = {0, 1};
    const char * names[2] = {"a_pos", "a_nrm"};
    if (gpu.CompileShaderProgram(kVs, kFs, &program, log, sizeof log, locs, names, 2)
        != static_cast<int>(GpuShaderCompileError::OK)) {
        std::cerr << "Shader compile failed:\n" << log << '\n';
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    auto * const glGetUniformLocationFn =
        reinterpret_cast<PFN_glGetUniformLocation>(gpu.GetGraphicsProc("glGetUniformLocation"));
    auto * const glUniformMatrix4fvFn =
        reinterpret_cast<PFN_glUniformMatrix4fv>(gpu.GetGraphicsProc("glUniformMatrix4fv"));
    auto * const glUniformMatrix3fvFn =
        reinterpret_cast<PFN_glUniformMatrix3fv>(gpu.GetGraphicsProc("glUniformMatrix3fv"));
    auto * const glUniform3fvFn = reinterpret_cast<PFN_glUniform3fv>(gpu.GetGraphicsProc("glUniform3fv"));
    auto * const glClearFn = reinterpret_cast<PFN_glClear>(gpu.GetGraphicsProc("glClear"));
    auto * const glDepthFuncFn = reinterpret_cast<PFN_glDepthFunc>(gpu.GetGraphicsProc("glDepthFunc"));
    auto * const glEnableFn = reinterpret_cast<PFN_glEnable>(gpu.GetGraphicsProc("glEnable"));
    auto * const glCullFaceFn = reinterpret_cast<PFN_glCullFace>(gpu.GetGraphicsProc("glCullFace"));
    auto * const glFrontFaceFn = reinterpret_cast<PFN_glFrontFace>(gpu.GetGraphicsProc("glFrontFace"));
    if (!glGetUniformLocationFn || !glUniformMatrix4fvFn || !glUniformMatrix3fvFn || !glUniform3fvFn || !glClearFn) {
        std::cerr << "Missing GL entry points.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    const GLuint glprog = static_cast<GLuint>(program);
    const GLint loc_mvp = glGetUniformLocationFn(glprog, "u_mvp");
    const GLint loc_nrm = glGetUniformLocationFn(glprog, "u_nrm");
    const GLint loc_light = glGetUniformLocationFn(glprog, "u_light");
    if (loc_mvp < 0 || loc_nrm < 0 || loc_light < 0) {
        std::cerr << "Shader uniform lookup failed.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    const int nv = (kGridN + 1) * (kGridN + 1);
    const float half = 42.f;
    std::vector<float> interleaved;
    interleaved.reserve(static_cast<std::size_t>(nv) * 6u);
    for (int j = 0; j <= kGridN; ++j) {
        const float tz = static_cast<float>(j) / static_cast<float>(kGridN);
        const float z = -half + tz * (2.f * half);
        for (int i = 0; i <= kGridN; ++i) {
            const float tx = static_cast<float>(i) / static_cast<float>(kGridN);
            const float x = -half + tx * (2.f * half);
            const float y = HeightAt(x, z);
            float nrm[3]{};
            NormalAt(x, z, nrm);
            interleaved.push_back(x);
            interleaved.push_back(y);
            interleaved.push_back(z);
            interleaved.push_back(nrm[0]);
            interleaved.push_back(nrm[1]);
            interleaved.push_back(nrm[2]);
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(kGridN) * static_cast<std::size_t>(kGridN) * 6u);
    for (int j = 0; j < kGridN; ++j) {
        for (int i = 0; i < kGridN; ++i) {
            const std::uint32_t i00 = static_cast<std::uint32_t>(i + j * (kGridN + 1));
            const std::uint32_t i10 = i00 + 1u;
            const std::uint32_t i01 = i00 + static_cast<std::uint32_t>(kGridN + 1);
            const std::uint32_t i11 = i01 + 1u;
            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i10);
            indices.push_back(i11);
            indices.push_back(i01);
        }
    }

    GpuObjectName vbo = 0;
    GpuObjectName ibo = 0;
    GpuObjectName vao = 0;
    if (gpu.CreateBuffer(&vbo) != 0 || gpu.CreateBuffer(&ibo) != 0 || gpu.CreateVertexArray(&vao) != 0) {
        std::cerr << "Buffer/VAO create failed.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }
    (void)gpu.BindVertexArray(vao);
    (void)gpu.BindBuffer(GpuBufferTarget::Vertex, vbo);
    (void)gpu.UploadBufferData(
        GpuBufferTarget::Vertex, interleaved.data(), interleaved.size() * sizeof(float), GpuBufferUsage::Static);
    (void)gpu.EnableVertexAttribute(0u);
    (void)gpu.SetVertexAttributeFloat(0, 3, 24, 0, false);
    (void)gpu.EnableVertexAttribute(1u);
    (void)gpu.SetVertexAttributeFloat(1, 3, 24, 12, false);
    (void)gpu.BindBuffer(GpuBufferTarget::Index, ibo);
    (void)gpu.UploadBufferData(
        GpuBufferTarget::Index, indices.data(), indices.size() * sizeof(std::uint32_t), GpuBufferUsage::Static);

    const float lightDir[3] = {0.32f, 0.78f, 0.42f};

    float yaw = 0.9f;
    float pitch = 0.45f;
    float dist = 95.f;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyUp = false;
    bool keyDown = false;
    bool keyPgUp = false;
    bool keyPgDn = false;

    bool running = true;
    backend.SetTargetFrameRate(60);
    std::uint64_t prevTick = backend.GetTicks();

    std::cout << "OpenGL heightmap — arrows orbit, PgUp/PgDn zoom, auto-rotate. Close to exit.\n";

    while (running) {
        HonkordGL::Events::Event ev{};
        while (HonkordGL::Events::PollEvent(ctx, ev)) {
            if (ev.type == HonkordGL::Events::EventType::QUIT)
                running = false;
            if (ev.type == HonkordGL::Events::EventType::RESIZE) {
                ctx.frame_buffer_width = ev.width;
                ctx.frame_buffer_height = ev.height;
                gpu.MakeCurrent();
                gpu.SetDefaultViewport();
            }
            if (ev.type == HonkordGL::Events::EventType::KEY_PRESS) {
                using HonkordGL::Events::KeyCode;
                if (ev.key == KeyCode::LEFT)
                    keyLeft = true;
                if (ev.key == KeyCode::RIGHT)
                    keyRight = true;
                if (ev.key == KeyCode::UP)
                    keyUp = true;
                if (ev.key == KeyCode::DOWN)
                    keyDown = true;
                if (ev.key == KeyCode::PAGE_UP)
                    keyPgUp = true;
                if (ev.key == KeyCode::PAGE_DOWN)
                    keyPgDn = true;
            }
            if (ev.type == HonkordGL::Events::EventType::KEY_RELEASE) {
                using HonkordGL::Events::KeyCode;
                if (ev.key == KeyCode::LEFT)
                    keyLeft = false;
                if (ev.key == KeyCode::RIGHT)
                    keyRight = false;
                if (ev.key == KeyCode::UP)
                    keyUp = false;
                if (ev.key == KeyCode::DOWN)
                    keyDown = false;
                if (ev.key == KeyCode::PAGE_UP)
                    keyPgUp = false;
                if (ev.key == KeyCode::PAGE_DOWN)
                    keyPgDn = false;
            }
        }

        const std::uint64_t nowTick = backend.GetTicks();
        const std::uint64_t tickDelta = nowTick >= prevTick ? (nowTick - prevTick) : 0;
        const float dt = static_cast<float>(tickDelta) / 60.f;
        prevTick = nowTick;

        yaw += 0.22f * dt;
        if (keyLeft)
            yaw -= 1.1f * dt;
        if (keyRight)
            yaw += 1.1f * dt;
        if (keyUp)
            pitch += 1.0f * dt;
        if (keyDown)
            pitch -= 1.0f * dt;
        pitch = std::clamp(pitch, 0.12f, 1.35f);
        if (keyPgUp)
            dist = std::max(28.f, dist - 42.f * dt);
        if (keyPgDn)
            dist = std::min(220.f, dist + 42.f * dt);

        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float eyeX = dist * cp * sy;
        const float eyeY = 8.f + dist * sp;
        const float eyeZ = dist * cp * cy;
        const float atY = 0.5f;

        float proj[16]{};
        float view[16]{};
        const int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
        const int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
        const float aspect = fbh > 0 ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.f;
        Mat4Perspective(58.f * 3.14159265f / 180.f, aspect, 0.5f, 320.f, proj);
        Mat4LookAtRh(eyeX, eyeY, eyeZ, 0.f, atY, 0.f, 0.f, 1.f, 0.f, view);
        float mvp[16]{};
        Mat4Mul(proj, view, mvp);

        const float rot[9] = {view[0], view[1], view[2], view[4], view[5], view[6], view[8], view[9], view[10]};

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.SetDepthTestEnabled(true);
        if (glDepthFuncFn)
            glDepthFuncFn(GL_LEQUAL);
        if (glEnableFn) {
            glEnableFn(GL_CULL_FACE);
            if (glCullFaceFn)
                glCullFaceFn(GL_BACK);
            if (glFrontFaceFn)
                glFrontFaceFn(GL_CCW);
        }
        gpu.ClearColorBuffer(0.04f, 0.06f, 0.1f, 1.f);
        glClearFn(GL_DEPTH_BUFFER_BIT);

        gpu.BindShaderProgram(program);
        glUniformMatrix4fvFn(loc_mvp, 1, GL_FALSE, mvp);
        glUniformMatrix3fvFn(loc_nrm, 1, GL_FALSE, rot);
        glUniform3fvFn(loc_light, 1, lightDir);

        (void)gpu.BindVertexArray(vao);
        (void)gpu.BindBuffer(GpuBufferTarget::Vertex, vbo);
        (void)gpu.BindBuffer(GpuBufferTarget::Index, ibo);
        (void)gpu.DrawIndexed(GpuPrimitive::Triangles, static_cast<int>(indices.size()), GpuIndexType::UInt32, 0);

        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    gpu.BindVertexArray(0);
    gpu.BindBuffer(GpuBufferTarget::Vertex, 0);
    gpu.BindBuffer(GpuBufferTarget::Index, 0);
    gpu.DestroyVertexArray(vao);
    gpu.DestroyBuffer(ibo);
    gpu.DestroyBuffer(vbo);
    gpu.DeleteShaderProgram(program);
    gpu.Destroy();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}
