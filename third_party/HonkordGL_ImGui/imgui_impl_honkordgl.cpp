/**
 * Dear ImGui — HonkordGL `GpuRenderer` + `Events` backend (OpenGL family).
 */

#include "imgui_impl_honkordgl.h"

#ifndef IMGUI_DISABLE

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

#include <cfloat>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if HONKORDGL_PLATFORM_APPLE
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#elif HONKORDGL_GPU_RENDERER_OPENGL_ES
#include <GLES3/gl3.h>
#elif HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#ifdef Status
#undef Status
#endif
#endif

using HonkordGL::Graphics::ApplicationContextSettings;
using HonkordGL::Graphics::GpuBufferTarget;
using HonkordGL::Graphics::GpuBufferUsage;
using HonkordGL::Graphics::GpuIndexType;
using HonkordGL::Graphics::GpuObjectName;
using HonkordGL::Graphics::GpuShaderCompileError;
using HonkordGL::Graphics::GpuRenderer;
using HonkordGL::Graphics::Renderers;
using HonkordGL::Graphics::RendererContextResult;

namespace {

struct ImGui_ImplHonkordGL_Data {
    ApplicationContextSettings * App{nullptr};
    GpuRenderer * Gpu{nullptr};
    GpuObjectName Shader{};
    GpuObjectName Vao{};
    GpuObjectName Vbo{};
    GpuObjectName Ibo{};
    GLint LocProjMtx{-1};
    GLint LocTexture{-1};
    bool HasVtxOffsetFn{false};
    ImDrawData * CurrentDrawData{nullptr};
    int FrameFbW{0};
    int FrameFbH{0};
    std::chrono::steady_clock::time_point LastFrame{};
    bool TimeValid{false};
};

static ImGui_ImplHonkordGL_Data * ImGui_ImplHonkordGL_GetBackendData()
{
    return ImGui::GetCurrentContext()
        ? static_cast<ImGui_ImplHonkordGL_Data *>(ImGui::GetIO().BackendRendererUserData)
        : nullptr;
}

static bool GlslPathOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}

#if HONKORDGL_PLATFORM_WINDOWS
using PFN_glDrawElementsBaseVertex =
    void(APIENTRY *)(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex);
using PFN_glActiveTexture = void(APIENTRY *)(GLenum);
using PFN_glBindTexture = void(APIENTRY *)(GLenum, GLuint);
using PFN_glBlendEquation = void(APIENTRY *)(GLenum);
using PFN_glBlendFuncSeparate = void(APIENTRY *)(GLenum, GLenum, GLenum, GLenum);
using PFN_glDisable = void(APIENTRY *)(GLenum);
using PFN_glEnable = void(APIENTRY *)(GLenum);
using PFN_glGetIntegerv = void(APIENTRY *)(GLenum, GLint *);
using PFN_glIsEnabled = GLboolean(APIENTRY *)(GLenum);
using PFN_glScissor = void(APIENTRY *)(GLint, GLint, GLsizei, GLsizei);
using PFN_glViewport = void(APIENTRY *)(GLint, GLint, GLsizei, GLsizei);
using PFN_glUseProgram = void(APIENTRY *)(GLuint);
using PFN_glUniform1i = void(APIENTRY *)(GLint, GLint);
using PFN_glUniformMatrix4fv = void(APIENTRY *)(GLint, GLsizei, GLboolean, const GLfloat *);
using PFN_glVertexAttribPointer = void(APIENTRY *)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
using PFN_glEnableVertexAttribArray = void(APIENTRY *)(GLuint);
using PFN_glDisableVertexAttribArray = void(APIENTRY *)(GLuint);
using PFN_glBindBuffer = void(APIENTRY *)(GLenum, GLuint);
using PFN_glTexParameteri = void(APIENTRY *)(GLenum, GLenum, GLint);
using PFN_glPixelStorei = void(APIENTRY *)(GLenum, GLint);
using PFN_glTexSubImage2D =
    void(APIENTRY *)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
#else
typedef void (*PFNGLDRAWELEMENTSBASEVERTEXPROC)(
    GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex);
#endif

static void ImGui_ImplHonkordGL_ResetRenderStateCb(ImDrawList const *, ImDrawCmd const *)
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!bd || !bd->Gpu || !bd->CurrentDrawData || bd->Shader == 0)
        return;
    if (bd->Gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glEnableFn = reinterpret_cast<PFN_glEnable>(bd->Gpu->GetGraphicsProc("glEnable"));
    auto * const glDisableFn = reinterpret_cast<PFN_glDisable>(bd->Gpu->GetGraphicsProc("glDisable"));
    auto * const glBlendEquationFn =
        reinterpret_cast<PFN_glBlendEquation>(bd->Gpu->GetGraphicsProc("glBlendEquation"));
    auto * const glBlendFuncSeparateFn =
        reinterpret_cast<PFN_glBlendFuncSeparate>(bd->Gpu->GetGraphicsProc("glBlendFuncSeparate"));
    auto * const glViewportFn = reinterpret_cast<PFN_glViewport>(bd->Gpu->GetGraphicsProc("glViewport"));
    auto * const glUseProgramFn = reinterpret_cast<PFN_glUseProgram>(bd->Gpu->GetGraphicsProc("glUseProgram"));
    auto * const glUniform1iFn = reinterpret_cast<PFN_glUniform1i>(bd->Gpu->GetGraphicsProc("glUniform1i"));
    auto * const glUniformMatrix4fvFn =
        reinterpret_cast<PFN_glUniformMatrix4fv>(bd->Gpu->GetGraphicsProc("glUniformMatrix4fv"));
    auto * const glBindBufferFn = reinterpret_cast<PFN_glBindBuffer>(bd->Gpu->GetGraphicsProc("glBindBuffer"));
    auto * const glEnableVertexAttribArrayFn =
        reinterpret_cast<PFN_glEnableVertexAttribArray>(bd->Gpu->GetGraphicsProc("glEnableVertexAttribArray"));
    auto * const glVertexAttribPointerFn =
        reinterpret_cast<PFN_glVertexAttribPointer>(bd->Gpu->GetGraphicsProc("glVertexAttribPointer"));
    if (!glEnableFn || !glBlendEquationFn || !glBlendFuncSeparateFn || !glViewportFn || !glUseProgramFn
        || !glUniformMatrix4fvFn || !glUniform1iFn || !glBindBufferFn || !glEnableVertexAttribArrayFn
        || !glVertexAttribPointerFn)
        return;
    glEnableFn(GL_BLEND);
    glBlendEquationFn(GL_FUNC_ADD);
    glBlendFuncSeparateFn(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisableFn(GL_CULL_FACE);
    glDisableFn(GL_DEPTH_TEST);
    glDisableFn(GL_STENCIL_TEST);
    glEnableFn(GL_SCISSOR_TEST);
    glViewportFn(0, 0, bd->FrameFbW, bd->FrameFbH);

    ImDrawData * const draw_data = bd->CurrentDrawData;
    float const L = draw_data->DisplayPos.x;
    float const R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float const T = draw_data->DisplayPos.y;
    float const B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgramFn(static_cast<GLuint>(bd->Shader));
    glUniform1iFn(bd->LocTexture, 0);
    glUniformMatrix4fvFn(bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (bd->Gpu->BindVertexArray(bd->Vao) != 0)
        return;
    glBindBufferFn(GL_ARRAY_BUFFER, static_cast<GLuint>(bd->Vbo));
    glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(bd->Ibo));
    glEnableVertexAttribArrayFn(0);
    glEnableVertexAttribArrayFn(1);
    glEnableVertexAttribArrayFn(2);
    glVertexAttribPointerFn(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, pos)));
    glVertexAttribPointerFn(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, uv)));
    glVertexAttribPointerFn(
        2,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(ImDrawVert),
        reinterpret_cast<void *>(offsetof(ImDrawVert, col)));
#else
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glViewport(0, 0, bd->FrameFbW, bd->FrameFbH);

    ImDrawData * const draw_data = bd->CurrentDrawData;
    float const L = draw_data->DisplayPos.x;
    float const R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float const T = draw_data->DisplayPos.y;
    float const B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgram(static_cast<GLuint>(bd->Shader));
    glUniform1i(bd->LocTexture, 0);
    glUniformMatrix4fv(bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (bd->Gpu->BindVertexArray(bd->Vao) != 0)
        return;
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(bd->Vbo));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(bd->Ibo));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, pos)));
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, uv)));
    glVertexAttribPointer(
        2,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(ImDrawVert),
        reinterpret_cast<void *>(offsetof(ImDrawVert, col)));
#endif
}

static ImGuiKey KeyCodeToImGui(HonkordGL::Events::KeyCode key) noexcept
{
    using KC = HonkordGL::Events::KeyCode;
    switch (key) {
    case KC::ESCAPE:
        return ImGuiKey_Escape;
    case KC::ENTER:
        return ImGuiKey_Enter;
    case KC::TAB:
        return ImGuiKey_Tab;
    case KC::SPACE:
        return ImGuiKey_Space;
    case KC::BACKQUOTE:
        return ImGuiKey_GraveAccent;
    case KC::SHIFT:
        return ImGuiKey_LeftShift;
    case KC::CTRL:
        return ImGuiKey_LeftCtrl;
    case KC::ALT:
        return ImGuiKey_LeftAlt;
    case KC::SUPER:
    case KC::META:
        return ImGuiKey_LeftSuper;
    case KC::CAPS_LOCK:
        return ImGuiKey_CapsLock;
    case KC::NUM_LOCK:
        return ImGuiKey_NumLock;
    case KC::PRINT_SCREEN:
        return ImGuiKey_PrintScreen;
    case KC::SCROLL_LOCK:
        return ImGuiKey_ScrollLock;
    case KC::PAUSE:
        return ImGuiKey_Pause;
    case KC::INSERT:
        return ImGuiKey_Insert;
    case KC::DELETE:
        return ImGuiKey_Delete;
    case KC::HOME:
        return ImGuiKey_Home;
    case KC::END:
        return ImGuiKey_End;
    case KC::PAGE_UP:
        return ImGuiKey_PageUp;
    case KC::PAGE_DOWN:
        return ImGuiKey_PageDown;
    case KC::UP:
        return ImGuiKey_UpArrow;
    case KC::DOWN:
        return ImGuiKey_DownArrow;
    case KC::LEFT:
        return ImGuiKey_LeftArrow;
    case KC::RIGHT:
        return ImGuiKey_RightArrow;
    case KC::BRACKET_LEFT:
        return ImGuiKey_LeftBracket;
    case KC::BRACKET_RIGHT:
        return ImGuiKey_RightBracket;
    case KC::SEMICOLON:
        return ImGuiKey_Semicolon;
    case KC::COMMA:
        return ImGuiKey_Comma;
    case KC::PERIOD:
        return ImGuiKey_Period;
    case KC::SLASH:
        return ImGuiKey_Slash;
    case KC::BACKSLASH:
        return ImGuiKey_Backslash;
    case KC::TILDE:
        return ImGuiKey_None;
    case KC::EQUAL:
        return ImGuiKey_Equal;
    case KC::MINUS:
        return ImGuiKey_Minus;
    default:
        break;
    }

    if (key >= KC::A && key <= KC::Z)
        return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + (static_cast<int>(key) - static_cast<int>(KC::A)));
    if (key >= KC::NUMBER_0 && key <= KC::NUMBER_9)
        return static_cast<ImGuiKey>(
            static_cast<int>(ImGuiKey_0) + (static_cast<int>(key) - static_cast<int>(KC::NUMBER_0)));
    if (key >= KC::F1 && key <= KC::F12)
        return static_cast<ImGuiKey>(
            static_cast<int>(ImGuiKey_F1) + (static_cast<int>(key) - static_cast<int>(KC::F1)));

    return ImGuiKey_None;
}

static int NativeMouseButtonToImGui(unsigned int btn) noexcept
{
    switch (btn) {
    case 1:
        return 0;
    case 2:
        return 2;
    case 3:
        return 1;
    default:
        return -1;
    }
}

static bool ExpandAlpha8ToRgba(ImTextureData * tex, std::vector<unsigned char> * out_rgba)
{
    if (!tex || tex->Format != ImTextureFormat_Alpha8 || !tex->Pixels)
        return false;
    const int n = tex->Width * tex->Height;
    out_rgba->resize(static_cast<std::size_t>(n) * 4u);
    unsigned char const * const src = tex->Pixels;
    unsigned char * const dst = out_rgba->data();
    for (int i = 0; i < n; ++i) {
        dst[i * 4 + 0] = 255;
        dst[i * 4 + 1] = 255;
        dst[i * 4 + 2] = 255;
        dst[i * 4 + 3] = src[i];
    }
    return true;
}

} // namespace

bool ImGui_ImplHonkordGL_Init(ApplicationContextSettings * app, GpuRenderer * gpu)
{
    IMGUI_CHECKVERSION();
    ImGuiIO & io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized?");
    IM_ASSERT(app != nullptr && gpu != nullptr);

    if (!gpu->SupportsTextureRgbUpload() || !GlslPathOk(app))
        return false;

    if (gpu->LoadShaderCompilerProcs() != static_cast<int>(GpuShaderCompileError::OK))
        return false;

    auto * const bd = IM_NEW(ImGui_ImplHonkordGL_Data)();
    io.BackendRendererUserData = bd;
    bd->App = app;
    bd->Gpu = gpu;

    io.BackendRendererName = "HonkordGL (OpenGL)";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();
    platform_io.DrawCallback_ResetRenderState = ImGui_ImplHonkordGL_ResetRenderStateCb;

#if !HONKORDGL_GPU_RENDERER_OPENGL_ES && !HONKORDGL_PLATFORM_APPLE
    if (gpu->MakeCurrent() == static_cast<int>(RendererContextResult::OK)) {
        auto * const drawBase = reinterpret_cast<PFNGLDRAWELEMENTSBASEVERTEXPROC>(
            gpu->GetGraphicsProc("glDrawElementsBaseVertex"));
        if (drawBase) {
            bd->HasVtxOffsetFn = true;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        }
    }
#elif HONKORDGL_PLATFORM_WINDOWS
    if (gpu->MakeCurrent() == static_cast<int>(RendererContextResult::OK)) {
        auto * const drawBase =
            reinterpret_cast<PFN_glDrawElementsBaseVertex>(gpu->GetGraphicsProc("glDrawElementsBaseVertex"));
        if (drawBase) {
            bd->HasVtxOffsetFn = true;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        }
    }
#endif

    if (!ImGui_ImplHonkordGL_CreateDeviceObjects())
        return false;

    return true;
}

void ImGui_ImplHonkordGL_Shutdown()
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!bd)
        return;

    ImGui_ImplHonkordGL_DestroyDeviceObjects();

    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();
    if (platform_io.DrawCallback_ResetRenderState == ImGui_ImplHonkordGL_ResetRenderStateCb)
        platform_io.DrawCallback_ResetRenderState = nullptr;
    platform_io.ClearRendererHandlers();

    ImGuiIO & io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);

    IM_DELETE(bd);
}

void ImGui_ImplHonkordGL_NewFrame(ApplicationContextSettings * app)
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplHonkordGL_Init?");

    ImGuiIO & io = ImGui::GetIO();
    if (!app) {
        io.DeltaTime = 1.0f / 60.0f;
        return;
    }

    int const fbw = app->frame_buffer_width > 0 ? app->frame_buffer_width : app->client_rect.w;
    int const fbh = app->frame_buffer_height > 0 ? app->frame_buffer_height : app->client_rect.z;
    io.DisplaySize = ImVec2(static_cast<float>(fbw), static_cast<float>(fbh));

    float const sx = app->dpi_x > 0 ? static_cast<float>(app->dpi_x) / 96.f : 1.f;
    float const sy = app->dpi_y > 0 ? static_cast<float>(app->dpi_y) / 96.f : sx;
    io.DisplayFramebufferScale = ImVec2(sx, sy);

    auto const now = std::chrono::steady_clock::now();
    if (!bd->TimeValid) {
        bd->TimeValid = true;
        bd->LastFrame = now;
        io.DeltaTime = 1.0f / 60.0f;
    } else {
        float const dt = std::chrono::duration<float>(now - bd->LastFrame).count();
        bd->LastFrame = now;
        io.DeltaTime = dt > 1e-6f ? dt : 1.0f / 60.0f;
    }

    if (!bd->Gpu || bd->Gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    if (bd->Shader == 0 && !ImGui_ImplHonkordGL_CreateDeviceObjects())
        IM_ASSERT(0 && "ImGui_ImplHonkordGL_CreateDeviceObjects failed");
}

bool ImGui_ImplHonkordGL_ProcessEvent(ApplicationContextSettings * app, HonkordGL::Events::Event const & ev)
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!bd || app != bd->App)
        return false;

    ImGuiIO & io = ImGui::GetIO();
    using ET = HonkordGL::Events::EventType;

    switch (ev.type) {
    case ET::QUIT:
        return false;
    case ET::RESIZE:
        return false;
    case ET::FOCUS:
        io.AddFocusEvent(ev.focused != 0);
        return false;
    case ET::MOUSE_ENTER:
        io.AddMousePosEvent(static_cast<float>(ev.x), static_cast<float>(ev.y));
        return false;
    case ET::MOUSE_LEAVE:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        return false;
    case ET::MOUSE_MOVE:
        io.AddMousePosEvent(static_cast<float>(ev.x), static_cast<float>(ev.y));
        return false;
    case ET::MOUSE_BUTTON_PRESS: {
        if (ev.mouse_button == 4u) {
            io.AddMouseWheelEvent(0.f, 1.f);
            return false;
        }
        if (ev.mouse_button == 5u) {
            io.AddMouseWheelEvent(0.f, -1.f);
            return false;
        }
        int const mb = NativeMouseButtonToImGui(ev.mouse_button);
        if (mb >= 0)
            io.AddMouseButtonEvent(mb, true);
        return false;
    }
    case ET::MOUSE_BUTTON_RELEASE: {
        if (ev.mouse_button == 4u || ev.mouse_button == 5u)
            return false;
        int const mb = NativeMouseButtonToImGui(ev.mouse_button);
        if (mb >= 0)
            io.AddMouseButtonEvent(mb, false);
        return false;
    }
    case ET::KEY_PRESS: {
        ImGuiKey const ik = KeyCodeToImGui(ev.key);
        if (ik != ImGuiKey_None)
            io.AddKeyEvent(ik, true);
        if (ev.character != 0u && !io.KeyCtrl && !io.KeySuper)
            io.AddInputCharacter(ev.character);
        return false;
    }
    case ET::KEY_RELEASE: {
        ImGuiKey const ik = KeyCodeToImGui(ev.key);
        if (ik != ImGuiKey_None)
            io.AddKeyEvent(ik, false);
        return false;
    }
    default:
        return false;
    }
}

void ImGui_ImplHonkordGL_UpdateTexture(ImTextureData * tex)
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!tex || !bd || !bd->Gpu || !GlslPathOk(bd->App))
        return;

    GpuRenderer * const gpu = bd->Gpu;
    if (gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glPixelStoreiFn =
        reinterpret_cast<PFN_glPixelStorei>(gpu->GetGraphicsProc("glPixelStorei"));
    auto * const glTexParameteriFn =
        reinterpret_cast<PFN_glTexParameteri>(gpu->GetGraphicsProc("glTexParameteri"));
    auto * const glTexSubImage2DFn =
        reinterpret_cast<PFN_glTexSubImage2D>(gpu->GetGraphicsProc("glTexSubImage2D"));
    auto * const glBindTextureFn =
        reinterpret_cast<PFN_glBindTexture>(gpu->GetGraphicsProc("glBindTexture"));
    if (glPixelStoreiFn)
        glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 1);
#else
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif

    if (tex->Status == ImTextureStatus_WantDestroy) {
        if (tex->TexID != ImTextureID_Invalid && tex->TexID != 0) {
            gpu->DestroyTexture(static_cast<GpuObjectName>(tex->TexID));
            tex->SetTexID(ImTextureID_Invalid);
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
        return;
    }

    std::vector<unsigned char> scratch_rgba;

    if (tex->Status == ImTextureStatus_WantCreate) {
        unsigned char const * upload_ptr = tex->Pixels;
        if (tex->Format == ImTextureFormat_Alpha8) {
            if (!ExpandAlpha8ToRgba(tex, &scratch_rgba))
                return;
            upload_ptr = scratch_rgba.data();
        }
        GpuObjectName gl_tex{};
        if (gpu->CreateTexture2DRgba8(tex->Width, tex->Height, upload_ptr, &gl_tex) != 0 || gl_tex == 0)
            return;
        tex->SetTexID(static_cast<ImTextureID>(gl_tex));
#if HONKORDGL_PLATFORM_WINDOWS
        if (glBindTextureFn && glTexParameteriFn) {
            glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
            glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTextureFn(GL_TEXTURE_2D, 0);
        }
#else
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
#endif
        tex->SetStatus(ImTextureStatus_OK);
        return;
    }

    if (tex->Status == ImTextureStatus_WantUpdates) {
        GpuObjectName const gl_tex = static_cast<GpuObjectName>(tex->TexID);
        if (gl_tex == 0)
            return;

        auto uploadBlock = [&](int x, int y, int w, int h) {
            if (w <= 0 || h <= 0 || !tex->Pixels)
                return;
            if (tex->Format == ImTextureFormat_RGBA32) {
                unsigned char const * src =
                    tex->Pixels + (x + y * tex->Width) * tex->BytesPerPixel;
#if HONKORDGL_PLATFORM_WINDOWS
                if (glBindTextureFn && glTexSubImage2DFn) {
                    glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
                    glTexSubImage2DFn(
                        GL_TEXTURE_2D,
                        0,
                        x,
                        y,
                        w,
                        h,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        src);
                    glBindTextureFn(GL_TEXTURE_2D, 0);
                }
#else
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
                glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, src);
                glBindTexture(GL_TEXTURE_2D, 0);
#endif
                return;
            }
            scratch_rgba.resize(static_cast<std::size_t>(w * h) * 4u);
            for (int row = 0; row < h; ++row) {
                unsigned char const * row_src =
                    tex->Pixels + (x + (y + row) * tex->Width) * tex->BytesPerPixel;
                unsigned char * row_dst = scratch_rgba.data() + row * static_cast<std::size_t>(w) * 4u;
                for (int col = 0; col < w; ++col) {
                    unsigned char const a = row_src[col];
                    row_dst[col * 4 + 0] = 255;
                    row_dst[col * 4 + 1] = 255;
                    row_dst[col * 4 + 2] = 255;
                    row_dst[col * 4 + 3] = a;
                }
            }
#if HONKORDGL_PLATFORM_WINDOWS
            if (glBindTextureFn && glTexSubImage2DFn) {
                glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
                glTexSubImage2DFn(
                    GL_TEXTURE_2D,
                    0,
                    x,
                    y,
                    w,
                    h,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    scratch_rgba.data());
                glBindTextureFn(GL_TEXTURE_2D, 0);
            }
#else
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex));
            glTexSubImage2D(
                GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, scratch_rgba.data());
            glBindTexture(GL_TEXTURE_2D, 0);
#endif
        };

        if (tex->Updates.Size > 0) {
            for (ImTextureRect const & r : tex->Updates)
                uploadBlock(r.x, r.y, r.w, r.h);
        } else {
            ImTextureRect const & u = tex->UpdateRect;
            uploadBlock(u.x, u.y, u.w, u.h);
        }

        tex->SetStatus(ImTextureStatus_OK);
    }
}

bool ImGui_ImplHonkordGL_CreateDeviceObjects()
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!bd || !bd->Gpu || !GlslPathOk(bd->App))
        return false;

    GpuRenderer * const gpu = bd->Gpu;
    if (gpu->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return false;

    ImGui_ImplHonkordGL_DestroyDeviceObjects();

    static char const kVs[] = R"(#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Color;
uniform mat4 ProjMtx;
out vec2 Frag_UV;
out vec4 Frag_Color;
void main()
{
    Frag_UV = UV;
    Frag_Color = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}
)";

    static char const kFs[] = R"(#version 330 core
in vec2 Frag_UV;
in vec4 Frag_Color;
uniform sampler2D Texture;
layout (location = 0) out vec4 Out_Color;
void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
)";

    char log[2048]{};
    GpuObjectName prog{};
    if (gpu->CompileShaderProgram(kVs, kFs, &prog, log, sizeof(log)) != 0 || prog == 0)
        return false;
    bd->Shader = prog;

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glGetUniformLocationFn =
        reinterpret_cast<GLint (*)(GLuint, const GLchar *)>(gpu->GetGraphicsProc("glGetUniformLocation"));
    if (!glGetUniformLocationFn)
        return false;
    bd->LocProjMtx = glGetUniformLocationFn(static_cast<GLuint>(prog), "ProjMtx");
    bd->LocTexture = glGetUniformLocationFn(static_cast<GLuint>(prog), "Texture");
#else
    bd->LocProjMtx = glGetUniformLocation(static_cast<GLuint>(prog), "ProjMtx");
    bd->LocTexture = glGetUniformLocation(static_cast<GLuint>(prog), "Texture");
#endif

    if (gpu->CreateVertexArray(&bd->Vao) != 0 || bd->Vao == 0)
        return false;
    if (gpu->CreateBuffer(&bd->Vbo) != 0 || bd->Vbo == 0)
        return false;
    if (gpu->CreateBuffer(&bd->Ibo) != 0 || bd->Ibo == 0)
        return false;

    return true;
}

void ImGui_ImplHonkordGL_DestroyDeviceObjects()
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!bd || !bd->Gpu)
        return;

    GpuRenderer * const gpu = bd->Gpu;
    (void)gpu->MakeCurrent();

    if (bd->Vao) {
        gpu->DestroyVertexArray(bd->Vao);
        bd->Vao = 0;
    }
    if (bd->Vbo) {
        gpu->DestroyBuffer(bd->Vbo);
        bd->Vbo = 0;
    }
    if (bd->Ibo) {
        gpu->DestroyBuffer(bd->Ibo);
        bd->Ibo = 0;
    }
    if (bd->Shader) {
        gpu->DeleteShaderProgram(bd->Shader);
        bd->Shader = 0;
    }
    bd->LocProjMtx = -1;
    bd->LocTexture = -1;
}

void ImGui_ImplHonkordGL_RenderDrawData(ImDrawData * draw_data, GpuRenderer * gpu)
{
    ImGui_ImplHonkordGL_Data * const bd = ImGui_ImplHonkordGL_GetBackendData();
    if (!draw_data || !bd || bd->Shader == 0)
        return;
    IM_ASSERT(gpu == bd->Gpu && "Pass the same GpuRenderer given to ImGui_ImplHonkordGL_Init");
    (void)gpu;

    int const fb_width =
        static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int const fb_height =
        static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    bd->CurrentDrawData = draw_data;
    bd->FrameFbW = fb_width;
    bd->FrameFbH = fb_height;

    GpuRenderer * const gr = bd->Gpu;
    if (gr->MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;

    if (draw_data->Textures != nullptr) {
        for (ImTextureData * tex : *draw_data->Textures) {
            if (tex && tex->Status != ImTextureStatus_OK)
                ImGui_ImplHonkordGL_UpdateTexture(tex);
        }
    }

#if HONKORDGL_PLATFORM_WINDOWS
    auto * const glEnableFn = reinterpret_cast<PFN_glEnable>(gr->GetGraphicsProc("glEnable"));
    auto * const glDisableFn = reinterpret_cast<PFN_glDisable>(gr->GetGraphicsProc("glDisable"));
    auto * const glBlendEquationFn =
        reinterpret_cast<PFN_glBlendEquation>(gr->GetGraphicsProc("glBlendEquation"));
    auto * const glBlendFuncSeparateFn =
        reinterpret_cast<PFN_glBlendFuncSeparate>(gr->GetGraphicsProc("glBlendFuncSeparate"));
    auto * const glViewportFn = reinterpret_cast<PFN_glViewport>(gr->GetGraphicsProc("glViewport"));
    auto * const glScissorFn = reinterpret_cast<PFN_glScissor>(gr->GetGraphicsProc("glScissor"));
    auto * const glActiveTextureFn =
        reinterpret_cast<PFN_glActiveTexture>(gr->GetGraphicsProc("glActiveTexture"));
    auto * const glBindTextureFn =
        reinterpret_cast<PFN_glBindTexture>(gr->GetGraphicsProc("glBindTexture"));
    auto * const glUseProgramFn = reinterpret_cast<PFN_glUseProgram>(gr->GetGraphicsProc("glUseProgram"));
    auto * const glUniform1iFn = reinterpret_cast<PFN_glUniform1i>(gr->GetGraphicsProc("glUniform1i"));
    auto * const glUniformMatrix4fvFn =
        reinterpret_cast<PFN_glUniformMatrix4fv>(gr->GetGraphicsProc("glUniformMatrix4fv"));
    auto * const glBindBufferFn = reinterpret_cast<PFN_glBindBuffer>(gr->GetGraphicsProc("glBindBuffer"));
    auto * const glEnableVertexAttribArrayFn =
        reinterpret_cast<PFN_glEnableVertexAttribArray>(gr->GetGraphicsProc("glEnableVertexAttribArray"));
    auto * const glDisableVertexAttribArrayFn =
        reinterpret_cast<PFN_glDisableVertexAttribArray>(gr->GetGraphicsProc("glDisableVertexAttribArray"));
    auto * const glVertexAttribPointerFn =
        reinterpret_cast<PFN_glVertexAttribPointer>(gr->GetGraphicsProc("glVertexAttribPointer"));
    PFN_glDrawElementsBaseVertex const glDrawElementsBaseVertexFn =
        reinterpret_cast<PFN_glDrawElementsBaseVertex>(
            gr->GetGraphicsProc("glDrawElementsBaseVertex"));
    auto * const glDrawElementsFn =
        reinterpret_cast<void(APIENTRY *)(GLenum, GLsizei, GLenum, const void *)>(
            gr->GetGraphicsProc("glDrawElements"));

    if (!glEnableFn || !glDisableFn || !glBlendEquationFn || !glBlendFuncSeparateFn || !glViewportFn
        || !glScissorFn || !glActiveTextureFn || !glBindTextureFn || !glUseProgramFn
        || !glUniformMatrix4fvFn || !glUniform1iFn || !glBindBufferFn || !glEnableVertexAttribArrayFn
        || !glVertexAttribPointerFn || !glDrawElementsFn)
        return;

    glEnableFn(GL_BLEND);
    glBlendEquationFn(GL_FUNC_ADD);
    glBlendFuncSeparateFn(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisableFn(GL_CULL_FACE);
    glDisableFn(GL_DEPTH_TEST);
    glDisableFn(GL_STENCIL_TEST);
    glEnableFn(GL_SCISSOR_TEST);
    glViewportFn(0, 0, fb_width, fb_height);

    ImVec2 const clip_off = draw_data->DisplayPos;
    ImVec2 const clip_scale = draw_data->FramebufferScale;

    float const L = draw_data->DisplayPos.x;
    float const R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float const T = draw_data->DisplayPos.y;
    float const B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgramFn(static_cast<GLuint>(bd->Shader));
    glUniform1iFn(bd->LocTexture, 0);
    glUniformMatrix4fvFn(bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (gr->BindVertexArray(bd->Vao) != 0)
        return;

    GLenum const idx_gl_type = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        ImDrawList const * const cmd_list = draw_data->CmdLists[n];

        if (gr->BindBuffer(GpuBufferTarget::Vertex, bd->Vbo) != 0)
            return;
        if (gr->UploadBufferData(
                GpuBufferTarget::Vertex,
                cmd_list->VtxBuffer.Data,
                static_cast<std::size_t>(cmd_list->VtxBuffer.Size) * sizeof(ImDrawVert),
                GpuBufferUsage::Stream)
            != 0)
            return;

        if (gr->BindBuffer(GpuBufferTarget::Index, bd->Ibo) != 0)
            return;
        if (gr->UploadBufferData(
                GpuBufferTarget::Index,
                cmd_list->IdxBuffer.Data,
                static_cast<std::size_t>(cmd_list->IdxBuffer.Size) * sizeof(ImDrawIdx),
                GpuBufferUsage::Stream)
            != 0)
            return;

        if (gr->BindVertexArray(bd->Vao) != 0)
            return;
        glBindBufferFn(GL_ARRAY_BUFFER, static_cast<GLuint>(bd->Vbo));
        glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(bd->Ibo));
        glEnableVertexAttribArrayFn(0);
        glEnableVertexAttribArrayFn(1);
        glEnableVertexAttribArrayFn(2);
        glVertexAttribPointerFn(
            0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, pos)));
        glVertexAttribPointerFn(
            1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, uv)));
        glVertexAttribPointerFn(
            2,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(ImDrawVert),
            reinterpret_cast<void *>(offsetof(ImDrawVert, col)));

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            ImDrawCmd const * const pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                if (platform_io.DrawCallback_ResetRenderState
                    && pcmd->UserCallback == platform_io.DrawCallback_ResetRenderState)
                    platform_io.DrawCallback_ResetRenderState(cmd_list, pcmd);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            ImVec4 const cr = pcmd->ClipRect;
            ImVec2 clip_min((cr.x - clip_off.x) * clip_scale.x, (cr.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((cr.z - clip_off.x) * clip_scale.x, (cr.w - clip_off.y) * clip_scale.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            glScissorFn(
                static_cast<int>(clip_min.x),
                static_cast<int>(static_cast<float>(fb_height) - clip_max.y),
                static_cast<int>(clip_max.x - clip_min.x),
                static_cast<int>(clip_max.y - clip_min.y));

            ImTextureID const tex_id = pcmd->GetTexID();
            glActiveTextureFn(GL_TEXTURE0);
            glBindTextureFn(GL_TEXTURE_2D, static_cast<GLuint>(tex_id));

            std::size_t const idx_offset =
                static_cast<std::size_t>(pcmd->IdxOffset) * sizeof(ImDrawIdx);
            void const * idx_ptr =
                reinterpret_cast<void const *>(static_cast<std::uintptr_t>(idx_offset));

            if (bd->HasVtxOffsetFn && glDrawElementsBaseVertexFn && pcmd->VtxOffset != 0)
                glDrawElementsBaseVertexFn(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(pcmd->ElemCount),
                    idx_gl_type,
                    idx_ptr,
                    static_cast<GLint>(pcmd->VtxOffset));
            else
                glDrawElementsFn(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(pcmd->ElemCount),
                    idx_gl_type,
                    idx_ptr);
        }
    }

    glDisableVertexAttribArrayFn(0);
    glDisableVertexAttribArrayFn(1);
    glDisableVertexAttribArrayFn(2);
    gr->BindShaderProgram(0);

#else
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glViewport(0, 0, fb_width, fb_height);

    ImVec2 const clip_off = draw_data->DisplayPos;
    ImVec2 const clip_scale = draw_data->FramebufferScale;

    float const L = draw_data->DisplayPos.x;
    float const R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float const T = draw_data->DisplayPos.y;
    float const B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float const ortho[4][4] = {
        {2.0f / (R - L), 0.f, 0.f, 0.f},
        {0.f, 2.0f / (T - B), 0.f, 0.f},
        {0.f, 0.f, -1.f, 0.f},
        {(R + L) / (L - R), (T + B) / (B - T), 0.f, 1.f},
    };

    glUseProgram(static_cast<GLuint>(bd->Shader));
    glUniform1i(bd->LocTexture, 0);
    glUniformMatrix4fv(bd->LocProjMtx, 1, GL_FALSE, &ortho[0][0]);

    if (gr->BindVertexArray(bd->Vao) != 0)
        return;

    auto * const glDrawElementsBaseVertexFn = reinterpret_cast<PFNGLDRAWELEMENTSBASEVERTEXPROC>(
        gr->GetGraphicsProc("glDrawElementsBaseVertex"));

    GLenum const idx_gl_type = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        ImDrawList const * const cmd_list = draw_data->CmdLists[n];

        if (gr->BindBuffer(GpuBufferTarget::Vertex, bd->Vbo) != 0)
            return;
        if (gr->UploadBufferData(
                GpuBufferTarget::Vertex,
                cmd_list->VtxBuffer.Data,
                static_cast<std::size_t>(cmd_list->VtxBuffer.Size) * sizeof(ImDrawVert),
                GpuBufferUsage::Stream)
            != 0)
            return;

        if (gr->BindBuffer(GpuBufferTarget::Index, bd->Ibo) != 0)
            return;
        if (gr->UploadBufferData(
                GpuBufferTarget::Index,
                cmd_list->IdxBuffer.Data,
                static_cast<std::size_t>(cmd_list->IdxBuffer.Size) * sizeof(ImDrawIdx),
                GpuBufferUsage::Stream)
            != 0)
            return;

        if (gr->BindVertexArray(bd->Vao) != 0)
            return;
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(bd->Vbo));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(bd->Ibo));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, pos)));
        glVertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void *>(offsetof(ImDrawVert, uv)));
        glVertexAttribPointer(
            2,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            sizeof(ImDrawVert),
            reinterpret_cast<void *>(offsetof(ImDrawVert, col)));

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            ImDrawCmd const * const pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                if (platform_io.DrawCallback_ResetRenderState
                    && pcmd->UserCallback == platform_io.DrawCallback_ResetRenderState)
                    platform_io.DrawCallback_ResetRenderState(cmd_list, pcmd);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            ImVec4 const cr = pcmd->ClipRect;
            ImVec2 clip_min((cr.x - clip_off.x) * clip_scale.x, (cr.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((cr.z - clip_off.x) * clip_scale.x, (cr.w - clip_off.y) * clip_scale.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            glScissor(
                static_cast<int>(clip_min.x),
                static_cast<int>(static_cast<float>(fb_height) - clip_max.y),
                static_cast<int>(clip_max.x - clip_min.x),
                static_cast<int>(clip_max.y - clip_min.y));

            ImTextureID const tex_id = pcmd->GetTexID();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex_id));

            std::size_t const idx_offset =
                static_cast<std::size_t>(pcmd->IdxOffset) * sizeof(ImDrawIdx);
            void const * idx_ptr =
                reinterpret_cast<void const *>(static_cast<std::uintptr_t>(idx_offset));

            if (bd->HasVtxOffsetFn && glDrawElementsBaseVertexFn && pcmd->VtxOffset != 0)
                glDrawElementsBaseVertexFn(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(pcmd->ElemCount),
                    idx_gl_type,
                    idx_ptr,
                    static_cast<GLint>(pcmd->VtxOffset));
            else
                glDrawElements(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(pcmd->ElemCount),
                    idx_gl_type,
                    idx_ptr);
        }
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    gr->BindShaderProgram(0);
#endif

    // OpenGL clears respect the scissor rect: leaving SCISSOR_TEST on makes the next
    // `ClearColorBuffer` wipe only the last ImGui clip box → smearing when the window moves.
#if HONKORDGL_PLATFORM_WINDOWS
    if (auto * const glDisableFn =
            reinterpret_cast<PFN_glDisable>(gr->GetGraphicsProc("glDisable"))) {
        glDisableFn(GL_SCISSOR_TEST);
    }
#else
    glDisable(GL_SCISSOR_TEST);
#endif
    gr->SetViewport(0, 0, fb_width, fb_height);

    bd->CurrentDrawData = nullptr;
}

#endif // IMGUI_DISABLE
