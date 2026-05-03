/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GpuRenderer implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuCapabilities.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdlib>
#include <cstdint>
#include <cstring>

#if HONKORDGL_PLATFORM_APPLE
#include <dlfcn.h>
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#elif HONKORDGL_GPU_RENDERER_OPENGL_ES
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#elif HONKORDGL_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
#include <EGL/egl.h>
#endif
#endif

#if HONKORDGL_PLATFORM_WINDOWS
#include "internal/D3D11RendererContextWin32.hpp"
#endif

namespace HonkordGL::Graphics {

/** Completes the forward declaration in `WindowApplication.h` (opaque to callers). */
struct HonkordGlGpuOpaqueContext {
    ApplicationContextSettings * app;
};

namespace {

GLenum GpuAdapterStringIdToGLenum(GpuAdapterStringId which) noexcept
{
    switch (which) {
    case GpuAdapterStringId::Vendor:
        return GL_VENDOR;
    case GpuAdapterStringId::Renderer:
        return GL_RENDERER;
    case GpuAdapterStringId::Version:
        return GL_VERSION;
    default:
        return 0;
    }
}

bool GlslGpuPathOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar != static_cast<int>(Renderers::VULKAN) && ar != static_cast<int>(Renderers::METAL)
        && ar != static_cast<int>(Renderers::DIRECT3D);
}

/** Portable RGBA texture upload path (OpenGL family or D3D11); excludes Vulkan/Metal swapchain-only attach. */
bool Rgba8TextureUploadBackendOk(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    if (ar == static_cast<int>(Renderers::VULKAN) || ar == static_cast<int>(Renderers::METAL))
        return false;
#if HONKORDGL_PLATFORM_WINDOWS
    if (ar == static_cast<int>(Renderers::DIRECT3D))
        return true;
#endif
    return GlslGpuPathOk(app);
}

#if HONKORDGL_PLATFORM_WINDOWS

using PFN_glGenTextures = void(APIENTRY*)(GLsizei, GLuint *);
using PFN_glBindTexture = void(APIENTRY*)(GLenum, GLuint);
using PFN_glTexParameteri = void(APIENTRY*)(GLenum, GLenum, GLint);
using PFN_glTexImage2D = void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
using PFN_glDeleteTextures = void(APIENTRY*)(GLsizei, const GLuint *);
using PFN_glActiveTexture = void(APIENTRY*)(GLenum);
using PFN_glUseProgram = void(APIENTRY*)(GLuint);
using PFN_glGenBuffers = void(APIENTRY*)(GLsizei, GLuint *);
using PFN_glDeleteBuffers = void(APIENTRY*)(GLsizei, const GLuint *);
using PFN_glBindBuffer = void(APIENTRY*)(GLenum, GLuint);
using PFN_glBufferData = void(APIENTRY*)(GLenum, GLsizeiptr, const void *, GLenum);
using PFN_glGenVertexArrays = void(APIENTRY*)(GLsizei, GLuint *);
using PFN_glDeleteVertexArrays = void(APIENTRY*)(GLsizei, const GLuint *);
using PFN_glBindVertexArray = void(APIENTRY*)(GLuint);
using PFN_glEnableVertexAttribArray = void(APIENTRY*)(GLuint);
using PFN_glVertexAttribPointer = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
using PFN_glDrawElements = void(APIENTRY*)(GLenum, GLsizei, GLenum, const void *);
using PFN_glEnable = void(APIENTRY*)(GLenum);
using PFN_glDisable = void(APIENTRY*)(GLenum);
using PFN_glDrawArrays = void(APIENTRY*)(GLenum, GLint, GLsizei);

#endif

GLenum ToGlBufferTarget(GpuBufferTarget target) noexcept
{
    switch (target) {
    case GpuBufferTarget::Vertex:
        return GL_ARRAY_BUFFER;
    case GpuBufferTarget::Index:
        return GL_ELEMENT_ARRAY_BUFFER;
    default:
        return 0;
    }
}

GLenum ToGlBufferUsage(GpuBufferUsage usage) noexcept
{
    switch (usage) {
    case GpuBufferUsage::Static:
        return GL_STATIC_DRAW;
    case GpuBufferUsage::Dynamic:
        return GL_DYNAMIC_DRAW;
    case GpuBufferUsage::Stream:
        return GL_STREAM_DRAW;
    default:
        return GL_STATIC_DRAW;
    }
}

GLenum ToGlPrimitive(GpuPrimitive primitive) noexcept
{
    switch (primitive) {
    case GpuPrimitive::Triangles:
        return GL_TRIANGLES;
    default:
        return 0;
    }
}

GLenum ToGlIndexType(GpuIndexType type) noexcept
{
    switch (type) {
    case GpuIndexType::UInt16:
        return GL_UNSIGNED_SHORT;
    case GpuIndexType::UInt32:
        return GL_UNSIGNED_INT;
    default:
        return 0;
    }
}

} // namespace

const char * RendererBackendLabel(Renderers backend) noexcept
{
    switch (backend) {
    case Renderers::NONE:
        return "NONE";
    case Renderers::OPENGL:
        return "OPENGL";
    case Renderers::EGL:
        return "EGL";
    case Renderers::METAL:
        return "METAL";
    case Renderers::WEBGPU:
        return "WEBGPU";
    case Renderers::WEBGL:
        return "WEBGL";
    case Renderers::VULKAN:
        return "VULKAN";
    case Renderers::DIRECT3D:
        return "DIRECT3D";
    default:
        return "UNKNOWN";
    }
}
GpuRenderer::GpuRenderer(ApplicationContextSettings& app) noexcept
    : app_(&app)
    , owns_attachment_(false)
{
}
GpuRenderer::GpuRenderer(GpuRenderer&& other) noexcept
    : app_(other.app_)
    , owns_attachment_(other.owns_attachment_)
{
    other.app_ = nullptr;
    other.owns_attachment_ = false;
}
GpuRenderer& GpuRenderer::operator=(GpuRenderer&& other) noexcept
{
    if (this != &other) {
        Destroy();
        app_ = other.app_;
        owns_attachment_ = other.owns_attachment_;
        other.app_ = nullptr;
        other.owns_attachment_ = false;
    }
    return *this;
}
GpuRenderer::~GpuRenderer() noexcept
{
    Destroy();
}
void GpuRenderer::Bind(ApplicationContextSettings& app) noexcept
{
    Destroy();
    app_ = &app;
    owns_attachment_ = false;
}
void GpuRenderer::Unbind() noexcept
{
    Destroy();
    app_ = nullptr;
}
int GpuRenderer::Create(const RendererContextSettings& spec) noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    if (!app_->window_handle)
        return static_cast<int>(RendererContextResult::NO_WINDOW);

    {
        const int vr = ValidateRendererDeviceRequest(spec);
        if (vr != static_cast<int>(RendererContextResult::OK))
            return vr;
    }

    if (owns_attachment_)
        Destroy();
    else if (app_->device != nullptr || app_->active_renderer != 0)
        DetachRendererContext(*app_);

    const int rc = AttachRendererContext(*app_, spec);
    if (rc == static_cast<int>(RendererContextResult::OK))
        owns_attachment_ = true;
    return rc;
}
int GpuRenderer::CreateAutomatic() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);

    if (NativePlatformUsesX11ClientHandles(static_cast<NativePlatform>(app_->native_platform))) {
        const char * const useEgl = std::getenv("HONKORDGL_USE_EGL");
        const bool preferEgl = useEgl && std::atoi(useEgl) != 0;

        RendererContextSettings eglSpec{};
        eglSpec.backend = Renderers::EGL;

        if (preferEgl) {
            int r = Create(eglSpec);
            if (r == 0)
                return 0;
            return Create(RendererContextSettings{});
        }

        int r = Create(RendererContextSettings{});
        if (r == 0)
            return 0;
        return Create(eglSpec);
    }

    return Create(RendererContextSettings{});
}
int GpuRenderer::CreateBestAvailable() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    const char * const preferVk = std::getenv("HONKORDGL_PREFER_VULKAN");
    const bool vkFirst = preferVk && std::atoi(preferVk) != 0;
    if (vkFirst) {
        int r = CreateVulkan();
        if (r == static_cast<int>(RendererContextResult::OK))
            return 0;
        return CreateAutomatic();
    }
    int r = CreateAutomatic();
    if (r == static_cast<int>(RendererContextResult::OK))
        return 0;
    return CreateVulkan();
#elif HONKORDGL_PLATFORM_APPLE
    int r = Create(RendererContextSettings{});
    if (r == static_cast<int>(RendererContextResult::OK))
        return 0;
    return CreateMetal();
#else
    return CreateAutomatic();
#endif
}
int GpuRenderer::CreateBestAvailableShaderBackend() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    const int r = CreateAutomatic();
#elif HONKORDGL_PLATFORM_APPLE
    const int r = Create(RendererContextSettings{});
#else
    const int r = CreateAutomatic();
#endif
    if (r != static_cast<int>(RendererContextResult::OK))
        return r;

    const Renderers b = ActiveBackend();
    if (b != Renderers::OPENGL && b != Renderers::EGL) {
        Destroy();
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }
    return 0;
}
int GpuRenderer::CreateVulkan() noexcept
{
    RendererContextSettings spec{};
    spec.backend = Renderers::VULKAN;
    return Create(spec);
}
int GpuRenderer::CreateMetal() noexcept
{
    RendererContextSettings spec{};
    spec.backend = Renderers::METAL;
    return Create(spec);
}

int GpuRenderer::CreateBestAvailableExhaustive() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    if (!app_->window_handle)
        return static_cast<int>(RendererContextResult::NO_WINDOW);

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    {
        const int r = CreateVulkan();
        if (r == static_cast<int>(RendererContextResult::OK))
            return 0;
    }
    {
        const int r = CreateAutomatic();
        if (r == static_cast<int>(RendererContextResult::OK))
            return 0;
    }
    {
        RendererContextSettings eglSpec{};
        eglSpec.backend = Renderers::EGL;
        return Create(eglSpec);
    }
#elif HONKORDGL_PLATFORM_APPLE
    {
        const int r = CreateMetal();
        if (r == static_cast<int>(RendererContextResult::OK))
            return 0;
    }
    return Create(RendererContextSettings{});
#elif HONKORDGL_PLATFORM_WINDOWS
    {
        RendererContextSettings d3d{};
        d3d.backend = Renderers::DIRECT3D;
        (void)Create(d3d);
    }
    return CreateAutomatic();
#elif HONKORDGL_PLATFORM_ANDROID
    return Create(RendererContextSettings{});
#else
    return CreateBestAvailable();
#endif
}

int GpuRenderer::CreateBestAvailableForCpuFramebufferPresent() noexcept
{
    return CreateBestAvailableShaderBackend();
}

bool GpuRenderer::SupportsTextureRgbUpload() const noexcept
{
    return Rgba8TextureUploadBackendOk(app_);
}

void GpuRenderer::Destroy() noexcept
{
    if (app_ && owns_attachment_) {
        DetachRendererContext(*app_);
        owns_attachment_ = false;
    }
}
bool GpuRenderer::IsCreated() const noexcept
{
    if (!app_ || !owns_attachment_)
        return false;
    const int ar = app_->active_renderer;
    if (ar == static_cast<int>(Renderers::METAL) || ar == static_cast<int>(Renderers::VULKAN)
        || ar == static_cast<int>(Renderers::DIRECT3D))
        return app_->renderer_private != nullptr;
    return app_->device != nullptr;
}
Renderers GpuRenderer::ActiveBackend() const noexcept
{
    if (!app_ || !owns_attachment_)
        return Renderers::NONE;
    return static_cast<Renderers>(static_cast<unsigned int>(app_->active_renderer));
}
const char * GpuRenderer::ActiveBackendLabel() const noexcept
{
    return RendererBackendLabel(ActiveBackend());
}
int GpuRenderer::MakeCurrent() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    return RendererContextMakeCurrent(*app_);
}
void GpuRenderer::SwapBuffers() noexcept
{
    if (!app_)
        return;
    RendererContextSwapBuffers(*app_);
    if (app_->gpu_frame_hook) {
        HonkordGlGpuOpaqueContext opaque{};
        opaque.app = app_;
        app_->gpu_frame_hook(&opaque, app_->gpu_frame_hook_user_data);
    }
}

int GpuRenderer::QueryHardwareLimits(GpuLimits * out_limits) noexcept
{
    return QueryGpuLimits(app_, out_limits);
}

int GpuRenderer::TryEnableOptionalFeature(GpuOptionalFeature feature) noexcept
{
    return TryEnableGpuOptionalFeature(app_, feature);
}

bool GpuRenderer::IsOptionalFeatureEnabled(GpuOptionalFeature feature) const noexcept
{
    return GpuOptionalFeatureIsEnabled(app_, feature);
}
void GpuRenderer::SetDefaultViewport() noexcept
{
    if (!app_)
        return;
    if (app_->active_renderer == static_cast<int>(Renderers::VULKAN)
        || app_->active_renderer == static_cast<int>(Renderers::METAL)
        || app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return;
    int w = app_->frame_buffer_width;
    int h = app_->frame_buffer_height;
    if (w <= 0 || h <= 0) {
        w = app_->client_rect.w;
        h = app_->client_rect.z;
    }
    if (w <= 0 || h <= 0)
        return;
    glViewport(0, 0, w, h);
}

void GpuRenderer::SetViewport(int x, int y, int width, int height) noexcept
{
    if (!app_)
        return;
    if (app_->active_renderer == static_cast<int>(Renderers::VULKAN)
        || app_->active_renderer == static_cast<int>(Renderers::METAL)
        || app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return;
    if (width <= 0 || height <= 0)
        return;
    glViewport(x, y, width, height);
}

void GpuRenderer::ClearColorBuffer(float r, float g, float b, float a) noexcept
{
    if (!app_)
        return;
    if (app_->active_renderer == static_cast<int>(Renderers::VULKAN)
        || app_->active_renderer == static_cast<int>(Renderers::METAL)
        || app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return;
    (void)MakeCurrent();
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const clearColor = reinterpret_cast<void (*)(GLfloat, GLfloat, GLfloat, GLfloat)>(GetGraphicsProc("glClearColor"));
    using ClearFn = void (*)(GLbitfield);
    auto * const clear = reinterpret_cast<ClearFn>(GetGraphicsProc("glClear"));
    if (clearColor)
        clearColor(r, g, b, a);
    if (clear)
        clear(GL_COLOR_BUFFER_BIT);
#else
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

int GpuRenderer::LoadShaderCompilerProcs() noexcept
{
    if (!app_)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
#if HONKORDGL_PLATFORM_WINDOWS
    if (app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return static_cast<int>(GpuShaderCompileError::OK);
#endif
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderLoadCompilerProcs();
}
void * RendererContextGetGraphicsProc(ApplicationContextSettings * app, const char * name) noexcept
{
    if (!name || !app)
        return nullptr;
#if HONKORDGL_PLATFORM_WINDOWS
    if (app->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return nullptr;
#endif
    (void)RendererContextMakeCurrent(*app);
#if HONKORDGL_PLATFORM_APPLE
    return dlsym(RTLD_DEFAULT, name);
#elif HONKORDGL_GPU_RENDERER_OPENGL_ES
    return reinterpret_cast<void *>(eglGetProcAddress(name));
#elif HONKORDGL_PLATFORM_WINDOWS
    void * p = reinterpret_cast<void *>(wglGetProcAddress(name));
    if (p)
        return p;
    return reinterpret_cast<void *>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));
#elif HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (app->active_renderer == static_cast<int>(Renderers::EGL) && app->egl_display)
        return reinterpret_cast<void *>(eglGetProcAddress(name));
    return reinterpret_cast<void *>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#else
    return reinterpret_cast<void *>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#endif
}

void * GpuRenderer::GetGraphicsProc(const char * name) noexcept
{
    if (!app_)
        return nullptr;
    return RendererContextGetGraphicsProc(app_, name);
}
int GpuRenderer::GetAdapterString(GpuAdapterStringId which, char * buf, std::size_t bufBytes) noexcept
{
    if (!buf || bufBytes == 0)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    buf[0] = '\0';
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    const GLenum name = GpuAdapterStringIdToGLenum(which);
    if (name == 0)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    const GLubyte * const s = glGetString(name);
    if (!s)
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    std::strncpy(buf, reinterpret_cast<const char *>(s), bufBytes - 1);
    buf[bufBytes - 1] = '\0';
    return static_cast<int>(GpuShaderCompileError::OK);
}
int GpuRenderer::GetAdapterVendor(char * buf, std::size_t bufBytes) noexcept
{
    return GetAdapterString(GpuAdapterStringId::Vendor, buf, bufBytes);
}
int GpuRenderer::GetAdapterVersion(char * buf, std::size_t bufBytes) noexcept
{
    return GetAdapterString(GpuAdapterStringId::Version, buf, bufBytes);
}
int GpuRenderer::GetAdapterRendererDescription(char * buf, std::size_t bufBytes) noexcept
{
    return GetAdapterString(GpuAdapterStringId::Renderer, buf, bufBytes);
}
int GpuRenderer::CreateTexture2DRgba8(
    int width,
    int height,
    void const * pixels,
    GpuObjectName * outTexture) noexcept
{
    if (!outTexture || width <= 0 || height <= 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outTexture = 0;
    if (!app_)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!Rgba8TextureUploadBackendOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
#if HONKORDGL_PLATFORM_WINDOWS
    if (app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return Internal::D3D11CreateTexture2DRgba8(*app_, width, height, pixels, outTexture);
#endif
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;

    GLuint id = 0;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#elif HONKORDGL_PLATFORM_APPLE
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#elif HONKORDGL_PLATFORM_WINDOWS
    {
        auto * const pstore = reinterpret_cast<void (*)(GLenum, GLint)>(GetGraphicsProc("glPixelStorei"));
        if (pstore)
            pstore(GL_UNPACK_ALIGNMENT, 1);
    }
    auto * const gen = reinterpret_cast<PFN_glGenTextures>(GetGraphicsProc("glGenTextures"));
    auto * const bind = reinterpret_cast<PFN_glBindTexture>(GetGraphicsProc("glBindTexture"));
    auto * const pari = reinterpret_cast<PFN_glTexParameteri>(GetGraphicsProc("glTexParameteri"));
    auto * const img = reinterpret_cast<PFN_glTexImage2D>(GetGraphicsProc("glTexImage2D"));
    if (!gen || !bind || !pari || !img)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    gen(1, &id);
    bind(GL_TEXTURE_2D, id);
    pari(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    pari(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    pari(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pari(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    img(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bind(GL_TEXTURE_2D, 0);
#else
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
    *outTexture = id;
    return static_cast<int>(GpuShaderCompileError::OK);
}

int GpuRenderer::UpdateTexture2DRgba8(
    GpuObjectName texture,
    int width,
    int height,
    void const * pixels) noexcept
{
    if (!texture || width <= 0 || height <= 0 || !pixels)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!app_)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (!Rgba8TextureUploadBackendOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
#if HONKORDGL_PLATFORM_WINDOWS
    if (app_->active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return Internal::D3D11UpdateTexture2DRgba8(*app_, texture, width, height, pixels);
#endif
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;

    const GLuint tid = static_cast<GLuint>(texture);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
    glBindTexture(GL_TEXTURE_2D, tid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#elif HONKORDGL_PLATFORM_APPLE
    glBindTexture(GL_TEXTURE_2D, tid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const bind = reinterpret_cast<PFN_glBindTexture>(GetGraphicsProc("glBindTexture"));
    auto * const pari = reinterpret_cast<PFN_glTexParameteri>(GetGraphicsProc("glTexParameteri"));
    using PFN_glPixelStorei = void(APIENTRY*)(GLenum, GLint);
    using PFN_glTexSubImage2D = void(APIENTRY*)(
        GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
    auto * const pstore = reinterpret_cast<PFN_glPixelStorei>(GetGraphicsProc("glPixelStorei"));
    auto * const sub = reinterpret_cast<PFN_glTexSubImage2D>(GetGraphicsProc("glTexSubImage2D"));
    if (!bind || !pstore || !sub)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bind(GL_TEXTURE_2D, tid);
    pstore(GL_UNPACK_ALIGNMENT, 1);
    sub(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bind(GL_TEXTURE_2D, 0);
#else
    glBindTexture(GL_TEXTURE_2D, tid);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
#endif
    return static_cast<int>(GpuShaderCompileError::OK);
}

void GpuRenderer::BindTexture2D(unsigned int textureUnit, GpuObjectName textureName) noexcept
{
    if (!app_ || !Rgba8TextureUploadBackendOk(app_))
        return;
#if HONKORDGL_PLATFORM_WINDOWS
    if (app_->active_renderer == static_cast<int>(Renderers::DIRECT3D)) {
        Internal::D3D11BindTexture2D(*app_, textureUnit, textureName);
        return;
    }
#endif
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    const GLenum tu = static_cast<GLenum>(GL_TEXTURE0 + textureUnit);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glActiveTexture(tu);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureName));
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const at = reinterpret_cast<PFN_glActiveTexture>(GetGraphicsProc("glActiveTexture"));
    auto * const bind = reinterpret_cast<PFN_glBindTexture>(GetGraphicsProc("glBindTexture"));
    if (at)
        at(tu);
    if (bind)
        bind(GL_TEXTURE_2D, static_cast<GLuint>(textureName));
#else
    glActiveTexture(tu);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureName));
#endif
}
void GpuRenderer::DestroyTexture(GpuObjectName textureName) noexcept
{
    if (!textureName || !app_ || !Rgba8TextureUploadBackendOk(app_))
        return;
#if HONKORDGL_PLATFORM_WINDOWS
    if (app_->active_renderer == static_cast<int>(Renderers::DIRECT3D)) {
        Internal::D3D11DestroyTexture(*app_, textureName);
        return;
    }
#endif
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    GLuint t = static_cast<GLuint>(textureName);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glDeleteTextures(1, &t);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const del = reinterpret_cast<PFN_glDeleteTextures>(GetGraphicsProc("glDeleteTextures"));
    if (del)
        del(1, &t);
#else
    glDeleteTextures(1, &t);
#endif
}
void GpuRenderer::BindShaderProgram(GpuObjectName program) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glUseProgram(static_cast<GLuint>(program));
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const use = reinterpret_cast<PFN_glUseProgram>(GetGraphicsProc("glUseProgram"));
    if (use)
        use(static_cast<GLuint>(program));
#else
    using PFN_glUseProgram_linux = void (*)(GLuint);
    static PFN_glUseProgram_linux useFn = nullptr;
    if (!useFn)
        useFn = reinterpret_cast<PFN_glUseProgram_linux>(GetGraphicsProc("glUseProgram"));
    if (useFn)
        useFn(static_cast<GLuint>(program));
#endif
}
int GpuRenderer::CompileShaderProgram(
    const char * vertexSource,
    const char * fragmentSource,
    GpuObjectName * outProgram,
    char * infoLog,
    std::size_t infoLogBytes) noexcept
{
    return CompileShaderProgram(
        vertexSource,
        fragmentSource,
        outProgram,
        infoLog,
        infoLogBytes,
        nullptr,
        nullptr,
        0);
}
int GpuRenderer::CompileShaderProgram(
    const char * vertexSource,
    const char * fragmentSource,
    GpuObjectName * outProgram,
    char * infoLog,
    std::size_t infoLogBytes,
    const int * attribLocations,
    const char * const * attribNames,
    int attribCount) noexcept
{
    if (!app_)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderCompileProgram(
        app_,
        vertexSource,
        fragmentSource,
        reinterpret_cast<unsigned int *>(outProgram),
        infoLog,
        infoLogBytes,
        attribLocations,
        attribNames,
        attribCount);
}
void GpuRenderer::DeleteShaderProgram(GpuObjectName program) noexcept
{
    if (!program || !app_)
        return;
    (void)MakeCurrent();
    GpuShaderDeleteProgram(static_cast<unsigned int>(program));
}

int GpuRenderer::CreateShaderObject(GpuShaderPipelineStage stage, GpuObjectName * outShader) noexcept
{
    if (!outShader || !app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outShader = 0;
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderCreateObject(app_, stage, reinterpret_cast<unsigned int *>(outShader));
}

int GpuRenderer::SetShaderSource(GpuObjectName shader, const char * source) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderSetShaderSource(app_, static_cast<unsigned int>(shader), source);
}

int GpuRenderer::CompileShaderObject(GpuObjectName shader, char * infoLog, std::size_t infoLogBytes) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderCompileShaderObject(app_, static_cast<unsigned int>(shader), infoLog, infoLogBytes);
}

int GpuRenderer::CreateProgramObject(GpuObjectName * outProgram) noexcept
{
    if (!outProgram || !app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outProgram = 0;
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderCreateProgramObject(app_, reinterpret_cast<unsigned int *>(outProgram));
}

void GpuRenderer::AttachShader(GpuObjectName program, GpuObjectName shader) noexcept
{
    if (!program || !shader || !app_ || !GlslGpuPathOk(app_))
        return;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    GpuShaderAttachShader(app_, static_cast<unsigned int>(program), static_cast<unsigned int>(shader));
}

int GpuRenderer::BindAttribLocation(GpuObjectName program, unsigned int location, const char * name) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderBindAttribLocation(app_, static_cast<unsigned int>(program), location, name);
}

int GpuRenderer::LinkShaderObjects(GpuObjectName program, char * infoLog, std::size_t infoLogBytes) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const int mc = MakeCurrent();
    if (mc != static_cast<int>(RendererContextResult::OK))
        return mc;
    return GpuShaderLinkProgramObject(app_, static_cast<unsigned int>(program), infoLog, infoLogBytes);
}

void GpuRenderer::DeleteShaderObject(GpuObjectName shader) noexcept
{
    if (!shader || !app_ || !GlslGpuPathOk(app_))
        return;
    (void)MakeCurrent();
    GpuShaderDeleteShaderObject(static_cast<unsigned int>(shader));
}

int GpuRenderer::CreateBuffer(GpuObjectName * outBuffer) noexcept
{
    if (!outBuffer || !app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outBuffer = 0;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);

    GLuint id = 0;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glGenBuffers(1, &id);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const gen = reinterpret_cast<PFN_glGenBuffers>(GetGraphicsProc("glGenBuffers"));
    if (!gen)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    gen(1, &id);
#else
    using PFN_gen = void (*)(GLsizei, GLuint *);
    static PFN_gen gen = nullptr;
    if (!gen)
        gen = reinterpret_cast<PFN_gen>(GetGraphicsProc("glGenBuffers"));
    if (!gen)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    gen(1, &id);
#endif
    *outBuffer = static_cast<GpuObjectName>(id);
    return 0;
}
void GpuRenderer::DestroyBuffer(GpuObjectName buffer) noexcept
{
    if (!buffer || !app_ || !GlslGpuPathOk(app_))
        return;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    const GLuint id = static_cast<GLuint>(buffer);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glDeleteBuffers(1, &id);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const del = reinterpret_cast<PFN_glDeleteBuffers>(GetGraphicsProc("glDeleteBuffers"));
    if (del)
        del(1, &id);
#else
    using PFN_del = void (*)(GLsizei, const GLuint *);
    static PFN_del del = nullptr;
    if (!del)
        del = reinterpret_cast<PFN_del>(GetGraphicsProc("glDeleteBuffers"));
    if (del)
        del(1, &id);
#endif
}
int GpuRenderer::BindBuffer(GpuBufferTarget target, GpuObjectName buffer) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    const GLenum glTarget = ToGlBufferTarget(target);
    if (!glTarget)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);

#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glBindBuffer(glTarget, static_cast<GLuint>(buffer));
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const bind = reinterpret_cast<PFN_glBindBuffer>(GetGraphicsProc("glBindBuffer"));
    if (!bind)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bind(glTarget, static_cast<GLuint>(buffer));
#else
    using PFN_bind = void (*)(GLenum, GLuint);
    static PFN_bind bind = nullptr;
    if (!bind)
        bind = reinterpret_cast<PFN_bind>(GetGraphicsProc("glBindBuffer"));
    if (!bind)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bind(glTarget, static_cast<GLuint>(buffer));
#endif
    return 0;
}
int GpuRenderer::UploadBufferData(
    GpuBufferTarget target,
    const void * data,
    std::size_t byteSize,
    GpuBufferUsage usage) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_) || (!data && byteSize > 0))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const GLenum glTarget = ToGlBufferTarget(target);
    if (!glTarget)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    const GLenum glUsage = ToGlBufferUsage(usage);
    const GLsizeiptr sz = static_cast<GLsizeiptr>(byteSize);

#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glBufferData(glTarget, sz, data, glUsage);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const fn = reinterpret_cast<PFN_glBufferData>(GetGraphicsProc("glBufferData"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(glTarget, sz, data, glUsage);
#else
    using PFN_buf = void (*)(GLenum, GLsizeiptr, const void *, GLenum);
    static PFN_buf fn = nullptr;
    if (!fn)
        fn = reinterpret_cast<PFN_buf>(GetGraphicsProc("glBufferData"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(glTarget, sz, data, glUsage);
#endif
    return 0;
}
int GpuRenderer::CreateVertexArray(GpuObjectName * outVertexArray) noexcept
{
    if (!outVertexArray || !app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    *outVertexArray = 0;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    GLuint id = 0;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glGenVertexArrays(1, &id);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const gen = reinterpret_cast<PFN_glGenVertexArrays>(GetGraphicsProc("glGenVertexArrays"));
    if (!gen)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    gen(1, &id);
#else
    using PFN_gen = void (*)(GLsizei, GLuint *);
    static PFN_gen gen = nullptr;
    if (!gen)
        gen = reinterpret_cast<PFN_gen>(GetGraphicsProc("glGenVertexArrays"));
    if (!gen)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    gen(1, &id);
#endif
    *outVertexArray = static_cast<GpuObjectName>(id);
    return 0;
}
void GpuRenderer::DestroyVertexArray(GpuObjectName vertexArray) noexcept
{
    if (!vertexArray || !app_ || !GlslGpuPathOk(app_))
        return;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
    const GLuint id = static_cast<GLuint>(vertexArray);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glDeleteVertexArrays(1, &id);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const del = reinterpret_cast<PFN_glDeleteVertexArrays>(GetGraphicsProc("glDeleteVertexArrays"));
    if (del)
        del(1, &id);
#else
    using PFN_del = void (*)(GLsizei, const GLuint *);
    static PFN_del del = nullptr;
    if (!del)
        del = reinterpret_cast<PFN_del>(GetGraphicsProc("glDeleteVertexArrays"));
    if (del)
        del(1, &id);
#endif
}
int GpuRenderer::BindVertexArray(GpuObjectName vertexArray) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glBindVertexArray(static_cast<GLuint>(vertexArray));
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const bind = reinterpret_cast<PFN_glBindVertexArray>(GetGraphicsProc("glBindVertexArray"));
    if (!bind)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bind(static_cast<GLuint>(vertexArray));
#else
    using PFN_bind = void (*)(GLuint);
    static PFN_bind bind = nullptr;
    if (!bind)
        bind = reinterpret_cast<PFN_bind>(GetGraphicsProc("glBindVertexArray"));
    if (!bind)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    bind(static_cast<GLuint>(vertexArray));
#endif
    return 0;
}
int GpuRenderer::EnableVertexAttribute(unsigned int location) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glEnableVertexAttribArray(static_cast<GLuint>(location));
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const fn = reinterpret_cast<PFN_glEnableVertexAttribArray>(GetGraphicsProc("glEnableVertexAttribArray"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(static_cast<GLuint>(location));
#else
    using PFN_fn = void (*)(GLuint);
    static PFN_fn fn = nullptr;
    if (!fn)
        fn = reinterpret_cast<PFN_fn>(GetGraphicsProc("glEnableVertexAttribArray"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(static_cast<GLuint>(location));
#endif
    return 0;
}
int GpuRenderer::SetVertexAttributeFloat(
    unsigned int location,
    int components,
    int strideBytes,
    std::size_t offsetBytes,
    bool normalized) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_) || components <= 0 || components > 4 || strideBytes < 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    const GLboolean glNorm = normalized ? GL_TRUE : GL_FALSE;
    const void * ptr = reinterpret_cast<const void *>(static_cast<std::uintptr_t>(offsetBytes));
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glVertexAttribPointer(static_cast<GLuint>(location), components, GL_FLOAT, glNorm, strideBytes, ptr);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const fn = reinterpret_cast<PFN_glVertexAttribPointer>(GetGraphicsProc("glVertexAttribPointer"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(static_cast<GLuint>(location), components, GL_FLOAT, glNorm, strideBytes, ptr);
#else
    using PFN_fn = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
    static PFN_fn fn = nullptr;
    if (!fn)
        fn = reinterpret_cast<PFN_fn>(GetGraphicsProc("glVertexAttribPointer"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(static_cast<GLuint>(location), components, GL_FLOAT, glNorm, strideBytes, ptr);
#endif
    return 0;
}
void GpuRenderer::SetDepthTestEnabled(bool enabled) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_))
        return;
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    if (enabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const en = reinterpret_cast<PFN_glEnable>(GetGraphicsProc("glEnable"));
    auto * const dis = reinterpret_cast<PFN_glDisable>(GetGraphicsProc("glDisable"));
    if (enabled) {
        if (en)
            en(GL_DEPTH_TEST);
    } else if (dis) {
        dis(GL_DEPTH_TEST);
    }
#else
    if (enabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
#endif
}

int GpuRenderer::DrawArrays(GpuPrimitive primitive, int firstVertex, int vertexCount) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_) || firstVertex < 0 || vertexCount < 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const GLenum glPrim = ToGlPrimitive(primitive);
    if (!glPrim)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glDrawArrays(glPrim, firstVertex, vertexCount);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const fn = reinterpret_cast<PFN_glDrawArrays>(GetGraphicsProc("glDrawArrays"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(glPrim, firstVertex, vertexCount);
#else
    glDrawArrays(glPrim, firstVertex, vertexCount);
#endif
    return 0;
}
int GpuRenderer::DrawIndexed(
    GpuPrimitive primitive,
    int indexCount,
    GpuIndexType indexType,
    std::size_t indexOffsetBytes) noexcept
{
    if (!app_ || !GlslGpuPathOk(app_) || indexCount < 0)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    const GLenum glPrim = ToGlPrimitive(primitive);
    const GLenum glIndexType = ToGlIndexType(indexType);
    if (!glPrim || !glIndexType)
        return static_cast<int>(GpuShaderCompileError::INVALID_ARGUMENT);
    if (MakeCurrent() != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(RendererContextResult::INVALID_ARGUMENT);
    const void * ptr = reinterpret_cast<const void *>(static_cast<std::uintptr_t>(indexOffsetBytes));
#if HONKORDGL_GPU_RENDERER_OPENGL_ES || HONKORDGL_PLATFORM_APPLE
    glDrawElements(glPrim, indexCount, glIndexType, ptr);
#elif HONKORDGL_PLATFORM_WINDOWS
    auto * const fn = reinterpret_cast<PFN_glDrawElements>(GetGraphicsProc("glDrawElements"));
    if (!fn)
        return static_cast<int>(GpuShaderCompileError::PROC_LOAD_FAILED);
    fn(glPrim, indexCount, glIndexType, ptr);
#else
    glDrawElements(glPrim, indexCount, glIndexType, ptr);
#endif
    return 0;
}

} // namespace HonkordGL::Graphics