/**
 * GPU ray-traced room (fragment shader ray–AABB): WASD to walk, arrow keys to look around; position
 * is clamped inside the walls. Uses HonkordGL `WindowBackend` + `GpuRenderer` (OpenGL 3.3 shaders).
 *
 * Example build (Linux + X11/Wayland, from repo root):
 *   gcc -c -o /tmp/xdg-shell.o src/Wayland/generated/xdg-shell-protocol.c
 *   g++ -std=c++17 -I include -I src -I src/Wayland \
 *     works/GPURaytracing/Main.cpp \
 *     works/MovingSquare/StbImageStub.cpp works/AsteroidGame/VulkanNoop.cpp \
 *     src/Video.cpp src/WindowBackend.cpp src/SoftwareRenderer.cpp \
 *     src/X11/WindowBackend.cpp src/X11/EventsX11.cpp src/X11/CursorX11.cpp \
 *     src/Wayland/EventsWayland.cpp src/Wayland/WindowBackend.cpp \
 *     src/Wayland/PlatformSession.cpp src/Wayland/IpcWayland.cpp \
 *     src/X11/RandrMonitorPoll.cpp src/X11/IpcHelperWindow.cpp src/X11/MonitorsX11.cpp \
 *     src/X11/GLXRendererContext.cpp src/X11/EGLRendererContextX11.cpp \
 *     src/Wayland/EGLRendererContextWayland.cpp \
 *     src/DRM/EventsDRM.cpp \
 *     src/GpuRenderer.cpp src/GpuShaderCompiler.cpp \
 *     src/Software2DContext.cpp src/SoftwareBlitCollector.cpp \
 *     /tmp/xdg-shell.o \
 *     -o works/GPURaytracing/GPURaytracing \
 *     -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl -lEGL -lGL
 */

#include <HonkordGL/Config.h>
#include <HonkordGL/Events.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuShaderCompiler.h>
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
#include <cstdlib>
#include <cstring>
#include <iostream>

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::CreateWindowOnSharedBackend;
using HonkordGL::Graphics::GpuBufferTarget;
using HonkordGL::Graphics::GpuObjectName;
using HonkordGL::Graphics::GpuPrimitive;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::GpuShaderCompileError;
using HonkordGL::Graphics::RendererContextResult;
using HonkordGL::Graphics::WindowBackend;

namespace
{

constexpr float kRoomMinX = -4.f;
constexpr float kRoomMaxX = 4.f;
constexpr float kRoomMinY = 0.f;
constexpr float kRoomMaxY = 3.f;
constexpr float kRoomMinZ = -4.f;
constexpr float kRoomMaxZ = 4.f;
constexpr float kEyeY = 1.55f;
constexpr float kWallMargin = 0.28f;
constexpr float kMoveSpeed = 4.5f;
constexpr float kTurnSpeed = 2.0f; // rad/s
constexpr float kPitchLimit = 1.48f; // ~85°
constexpr float kFovDeg = 68.f;

static float ClampXZ(float x, float z, float & outX, float & outZ) noexcept
{
    const float minX = kRoomMinX + kWallMargin;
    const float maxX = kRoomMaxX - kWallMargin;
    const float minZ = kRoomMinZ + kWallMargin;
    const float maxZ = kRoomMaxZ - kWallMargin;
    outX = std::clamp(x, minX, maxX);
    outZ = std::clamp(z, minZ, maxZ);
    return kEyeY;
}

constexpr char kVs[] = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
  v_uv = a_pos * 0.5 + 0.5;
  gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

constexpr char kFs[] = R"(#version 330 core
in vec2 v_uv;
out vec4 o_color;

uniform vec3 u_cam_pos;
uniform mat3 u_cam_basis;
uniform vec3 u_room_min;
uniform vec3 u_room_max;
uniform vec3 u_light_dir;
uniform float u_aspect;
uniform float u_tan_half_fovy;

bool hit_box_exit(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float t_hit, out vec3 n) {
  t_hit = 1e30;
  n = vec3(0.0);
  bool found = false;

  if (abs(rd.x) > 1e-8) {
    float tx = (rd.x > 0.0) ? (bmax.x - ro.x) / rd.x : (bmin.x - ro.x) / rd.x;
    if (tx > 1e-6) {
      vec3 p = ro + rd * tx;
      if (p.y >= bmin.y && p.y <= bmax.y && p.z >= bmin.z && p.z <= bmax.z && tx < t_hit) {
        t_hit = tx;
        n = (rd.x > 0.0) ? vec3(-1.0, 0.0, 0.0) : vec3(1.0, 0.0, 0.0);
        found = true;
      }
    }
  }
  if (abs(rd.y) > 1e-8) {
    float ty = (rd.y > 0.0) ? (bmax.y - ro.y) / rd.y : (bmin.y - ro.y) / rd.y;
    if (ty > 1e-6) {
      vec3 p = ro + rd * ty;
      if (p.x >= bmin.x && p.x <= bmax.x && p.z >= bmin.z && p.z <= bmax.z && ty < t_hit) {
        t_hit = ty;
        n = (rd.y > 0.0) ? vec3(0.0, -1.0, 0.0) : vec3(0.0, 1.0, 0.0);
        found = true;
      }
    }
  }
  if (abs(rd.z) > 1e-8) {
    float tz = (rd.z > 0.0) ? (bmax.z - ro.z) / rd.z : (bmin.z - ro.z) / rd.z;
    if (tz > 1e-6) {
      vec3 p = ro + rd * tz;
      if (p.x >= bmin.x && p.x <= bmax.x && p.y >= bmin.y && p.y <= bmax.y && tz < t_hit) {
        t_hit = tz;
        n = (rd.z > 0.0) ? vec3(0.0, 0.0, -1.0) : vec3(0.0, 0.0, 1.0);
        found = true;
      }
    }
  }
  return found;
}

void main() {
  vec2 ndc = v_uv * 2.0 - 1.0;
  ndc.x *= u_aspect;
  vec3 dir_cam = vec3(ndc.x * u_tan_half_fovy, ndc.y * u_tan_half_fovy, 1.0);
  vec3 rd = normalize(u_cam_basis * dir_cam);

  float t;
  vec3 n;
  if (!hit_box_exit(u_cam_pos, rd, u_room_min, u_room_max, t, n)) {
    o_color = vec4(0.02, 0.03, 0.06, 1.0);
    return;
  }

  vec3 L = normalize(-u_light_dir);
  float diff = max(dot(n, L), 0.0);
  vec3 base = vec3(0.72, 0.7, 0.68);
  if (n.y > 0.5) {
    vec3 p = u_cam_pos + rd * t;
    float cx = floor(p.x * 0.5) + floor(p.z * 0.5);
    float chk = mod(cx, 2.0);
    base = mix(vec3(0.35, 0.32, 0.38), vec3(0.55, 0.52, 0.48), chk);
  } else if (n.y < -0.5) {
    base = vec3(0.45, 0.42, 0.5);
  } else {
    base = vec3(0.62, 0.58, 0.55);
  }

  vec3 amb = 0.12 * base;
  vec3 col = amb + 0.88 * diff * base;
  o_color = vec4(col, 1.0);
}
)";

using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar *);
using PFN_glUniform3fv = void (*)(GLint, GLsizei, const GLfloat *);
using PFN_glUniformMatrix3fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glUniform1f = void (*)(GLint, GLfloat);

static const float kTri[] = {-1.f, -1.f, 3.f, -1.f, -1.f, 3.f};

} // namespace

int main()
{
    WindowBackend backend;
    ApplicationContextSettings settings{};
    settings.window_title = "HonkordGL — GPU ray-traced room (WASD + arrows)";
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

    GpuObjectName program = 0;
    char log[2048]{};
    const int locs[1] = {0};
    const char * names[1] = {"a_pos"};
    if (gpu.CompileShaderProgram(kVs, kFs, &program, log, sizeof log, locs, names, 1)
        != static_cast<int>(GpuShaderCompileError::OK)) {
        std::cerr << "Shader compile failed:\n" << log << '\n';
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    auto * const glGetUniformLocationFn =
        reinterpret_cast<PFN_glGetUniformLocation>(gpu.GetGraphicsProc("glGetUniformLocation"));
    auto * const glUniform3fvFn = reinterpret_cast<PFN_glUniform3fv>(gpu.GetGraphicsProc("glUniform3fv"));
    auto * const glUniformMatrix3fvFn =
        reinterpret_cast<PFN_glUniformMatrix3fv>(gpu.GetGraphicsProc("glUniformMatrix3fv"));
    auto * const glUniform1fFn = reinterpret_cast<PFN_glUniform1f>(gpu.GetGraphicsProc("glUniform1f"));
    if (!glGetUniformLocationFn || !glUniform3fvFn || !glUniformMatrix3fvFn || !glUniform1fFn) {
        std::cerr << "Missing GL uniform entry points.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    const GLuint glprog = static_cast<GLuint>(program);
    const GLint loc_cam = glGetUniformLocationFn(glprog, "u_cam_pos");
    const GLint loc_basis = glGetUniformLocationFn(glprog, "u_cam_basis");
    const GLint loc_rmin = glGetUniformLocationFn(glprog, "u_room_min");
    const GLint loc_rmax = glGetUniformLocationFn(glprog, "u_room_max");
    const GLint loc_ldir = glGetUniformLocationFn(glprog, "u_light_dir");
    const GLint loc_aspect = glGetUniformLocationFn(glprog, "u_aspect");
    const GLint loc_tan = glGetUniformLocationFn(glprog, "u_tan_half_fovy");
    if (loc_cam < 0 || loc_basis < 0 || loc_rmin < 0 || loc_rmax < 0 || loc_ldir < 0 || loc_aspect < 0
        || loc_tan < 0) {
        std::cerr << "Shader uniform lookup failed.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }

    GpuObjectName vbo = 0;
    GpuObjectName vao = 0;
    if (gpu.CreateBuffer(&vbo) != 0 || gpu.CreateVertexArray(&vao) != 0) {
        std::cerr << "Buffer/VAO create failed.\n";
        gpu.DeleteShaderProgram(program);
        gpu.Destroy();
        window->TerminateWindow();
        backend.Shutdown();
        return 1;
    }
    (void)gpu.BindVertexArray(vao);
    (void)gpu.BindBuffer(GpuBufferTarget::Vertex, vbo);
    (void)gpu.UploadBufferData(GpuBufferTarget::Vertex, kTri, sizeof(kTri), HonkordGL::Graphics::GpuBufferUsage::Static);
    (void)gpu.EnableVertexAttribute(0u);
    (void)gpu.SetVertexAttributeFloat(0, 2, 8, 0, false);

    const float roomMin[3] = {kRoomMinX, kRoomMinY, kRoomMinZ};
    const float roomMax[3] = {kRoomMaxX, kRoomMaxY, kRoomMaxZ};
    const float lightDir[3] = {0.25f, 0.85f, 0.35f};

    float px = 0.f;
    float pz = 0.f;
    float py = kEyeY;
    (void)ClampXZ(px, pz, px, pz);

    float yaw = 0.f;
    float pitch = 0.f;

    bool keyW = false;
    bool keyA = false;
    bool keyS = false;
    bool keyD = false;
    bool keyLeft = false;
    bool keyRight = false;
    bool keyUp = false;
    bool keyDown = false;

    bool running = true;
    backend.SetTargetFrameRate(60);
    std::uint64_t prevTick = backend.GetTicks();

    std::cout << "GPU ray-traced room — WASD move, arrow keys look, stay inside walls. Close to exit.\n";

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
                if (ev.key == KeyCode::W)
                    keyW = true;
                if (ev.key == KeyCode::A)
                    keyA = true;
                if (ev.key == KeyCode::S)
                    keyS = true;
                if (ev.key == KeyCode::D)
                    keyD = true;
                if (ev.key == KeyCode::LEFT)
                    keyLeft = true;
                if (ev.key == KeyCode::RIGHT)
                    keyRight = true;
                if (ev.key == KeyCode::UP)
                    keyUp = true;
                if (ev.key == KeyCode::DOWN)
                    keyDown = true;
            }
            if (ev.type == HonkordGL::Events::EventType::KEY_RELEASE) {
                using HonkordGL::Events::KeyCode;
                if (ev.key == KeyCode::W)
                    keyW = false;
                if (ev.key == KeyCode::A)
                    keyA = false;
                if (ev.key == KeyCode::S)
                    keyS = false;
                if (ev.key == KeyCode::D)
                    keyD = false;
                if (ev.key == KeyCode::LEFT)
                    keyLeft = false;
                if (ev.key == KeyCode::RIGHT)
                    keyRight = false;
                if (ev.key == KeyCode::UP)
                    keyUp = false;
                if (ev.key == KeyCode::DOWN)
                    keyDown = false;
            }
        }

        const std::uint64_t nowTick = backend.GetTicks();
        const std::uint64_t tickDelta = nowTick >= prevTick ? (nowTick - prevTick) : 0;
        const float dt = static_cast<float>(tickDelta) / 60.f;
        prevTick = nowTick;

        if (keyLeft)
            yaw += kTurnSpeed * dt;
        if (keyRight)
            yaw -= kTurnSpeed * dt;
        if (keyUp)
            pitch += kTurnSpeed * dt;
        if (keyDown)
            pitch -= kTurnSpeed * dt;
        pitch = std::clamp(pitch, -kPitchLimit, kPitchLimit);

        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        // Forward where camera looks (into the scene), Y-up
        const float fwx = sy * cp;
        const float fwy = sp;
        const float fwz = -cy * cp;
        // Walk on XZ plane: forward / right
        float mx = fwx;
        float mz = fwz;
        const float mh = std::sqrt(mx * mx + mz * mz);
        if (mh > 1e-6f) {
            mx /= mh;
            mz /= mh;
        }
        const float sx = -mz;
        const float sz = mx;

        const float in_fwd = (keyS ? 1.f : 0.f) - (keyW ? 1.f : 0.f);
        const float in_rt = (keyD ? 1.f : 0.f) - (keyA ? 1.f : 0.f);
        px += (-mx * in_fwd + sx * in_rt) * kMoveSpeed * dt;
        pz += (-mz * in_fwd + sz * in_rt) * kMoveSpeed * dt;
        py = ClampXZ(px, pz, px, pz);

        // Orthonormal basis: right, up, forward (forward = look direction into scene)
        float fx = fwx;
        float fy = fwy;
        float fz = fwz;
        const float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (fl > 1e-6f) {
            fx /= fl;
            fy /= fl;
            fz /= fl;
        }
        // right = normalize(forward × world_up), world_up = (0,1,0)  =>  (-fz, 0, fx)
        float rx = -fz;
        float ry = 0.f;
        float rz = fx;
        const float rl = std::sqrt(rx * rx + rz * rz);
        if (rl > 1e-6f) {
            rx /= rl;
            rz /= rl;
        } else {
            rx = 1.f;
            rz = 0.f;
        }
        // up = right × forward
        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;
        const float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
        if (ul > 1e-6f) {
            ux /= ul;
            uy /= ul;
            uz /= ul;
        }
        // Column-major mat3: columns = right, up, forward
        const float basis9[9] = {rx, ry, rz, ux, uy, uz, fx, fy, fz};

        const int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
        const int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
        const float aspect = fbh > 0 ? static_cast<float>(fbw) / static_cast<float>(fbh) : 1.f;
        const float tanHalf = std::tan(kFovDeg * 3.14159265f / 180.f * 0.5f);

        const float cam[3] = {px, py, pz};

        gpu.MakeCurrent();
        gpu.SetDefaultViewport();
        gpu.SetDepthTestEnabled(false);
        gpu.ClearColorBuffer(0.02f, 0.03f, 0.05f, 1.f);
        gpu.BindShaderProgram(program);

        glUniform3fvFn(loc_cam, 1, cam);
        glUniformMatrix3fvFn(loc_basis, 1, GL_FALSE, basis9);
        glUniform3fvFn(loc_rmin, 1, roomMin);
        glUniform3fvFn(loc_rmax, 1, roomMax);
        glUniform3fvFn(loc_ldir, 1, lightDir);
        glUniform1fFn(loc_aspect, aspect);
        glUniform1fFn(loc_tan, tanHalf);

        (void)gpu.BindVertexArray(vao);
        (void)gpu.DrawArrays(GpuPrimitive::Triangles, 0, 3);
        gpu.SwapBuffers();
        backend.DelayFrame();
    }

    gpu.BindVertexArray(0);
    gpu.BindBuffer(GpuBufferTarget::Vertex, 0);
    gpu.DestroyVertexArray(vao);
    gpu.DestroyBuffer(vbo);
    gpu.DeleteShaderProgram(program);
    gpu.Destroy();
    window->TerminateWindow();
    backend.Shutdown();
    return 0;
}