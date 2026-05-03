/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — OpenGL integration helpers (implementation)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/OpenGlIntegration.h>
#include <HonkordGL/PlatformAdapter.h>

#include <cstdio>
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
#endif

namespace HonkordGL::Graphics {

namespace {

bool OpenGlFamilyActive(ApplicationContextSettings const * app) noexcept
{
    if (!app)
        return false;
    const int ar = app->active_renderer;
    return ar == static_cast<int>(Renderers::OPENGL) || ar == static_cast<int>(Renderers::EGL);
}

void ZeroHints(OpenGlIntegrationHints * h) noexcept
{
    if (!h)
        return;
    std::memset(h, 0, sizeof(OpenGlIntegrationHints));
}

void ReadGlVersionInts(int * major, int * minor) noexcept
{
    *major = 0;
    *minor = 0;
#ifdef GL_MAJOR_VERSION
    GLint mj = 0;
    GLint mn = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &mj);
    glGetIntegerv(GL_MINOR_VERSION, &mn);
    if (mj > 0) {
        *major = mj;
        *minor = mn;
        return;
    }
#endif
    const GLubyte * const ver = glGetString(GL_VERSION);
    if (!ver)
        return;
    int a = 0;
    int b = 0;
#if HONKORDGL_GPU_RENDERER_OPENGL_ES
    if (std::sscanf(reinterpret_cast<const char *>(ver), "OpenGL ES %d.%d", &a, &b) == 2) {
        *major = a;
        *minor = b;
        return;
    }
#endif
    if (std::sscanf(reinterpret_cast<const char *>(ver), "%d.%d", &a, &b) >= 2) {
        *major = a;
        *minor = b;
    }
}

} // namespace

bool OpenGlIntegrationIsActive(ApplicationContextSettings const * app) noexcept
{
    return OpenGlFamilyActive(app);
}

void OpenGlIntegrationGetNativeRenderingHandles(
    ApplicationContextSettings const * app,
    OpenGlNativeRenderingHandles * out) noexcept
{
    if (!out)
        return;
    std::memset(out, 0, sizeof(OpenGlNativeRenderingHandles));
    if (!app || !OpenGlFamilyActive(app))
        return;
    out->rendering_context = app->device;
    out->platform_display_or_connection = app->device_context;
    out->drawable_surface = app->surface;
    out->egl_display_if_any = app->egl_display;
    out->window_host = app->window_handle;
}

int OpenGlIntegrationQueryHints(ApplicationContextSettings * app, OpenGlIntegrationHints * out_hints) noexcept
{
    if (!app || !out_hints)
        return static_cast<int>(OpenGlIntegrationResult::INVALID_ARGUMENT);
    ZeroHints(out_hints);
    if (!OpenGlFamilyActive(app))
        return static_cast<int>(OpenGlIntegrationResult::UNSUPPORTED_BACKEND);
    if (!app->device)
        return static_cast<int>(OpenGlIntegrationResult::NO_CONTEXT);

    const int mc = RendererContextMakeCurrent(*app);
    if (mc != static_cast<int>(RendererContextResult::OK))
        return static_cast<int>(OpenGlIntegrationResult::MAKE_CURRENT_FAILED);

    if (app->active_renderer == static_cast<int>(Renderers::EGL))
        out_hints->api_kind = OpenGlIntegrationApiKind::EglGl;
    else
        out_hints->api_kind = OpenGlIntegrationApiKind::DesktopGl;

    ReadGlVersionInts(&out_hints->gl_major_version, &out_hints->gl_minor_version);

    out_hints->effective_color_bits = app->color_bits;
    out_hints->effective_depth_bits = app->depth_bits;
    out_hints->effective_stencil_bits = app->stencil_bits;
    out_hints->effective_msaa_samples = app->msaa_samples;
    out_hints->double_buffered = 1;
    out_hints->vsync_enabled = app->vsync_enabled;
    out_hints->framebuffer_width = app->frame_buffer_width;
    out_hints->framebuffer_height = app->frame_buffer_height;
    return static_cast<int>(OpenGlIntegrationResult::OK);
}

void OpenGlIntegrationRecommendAttachHints(
    RendererContextSettings * spec,
    Renderers backend,
    int gl_major,
    int gl_minor,
    int core_profile,
    int forward_compatible,
    int double_buffer,
    int vsync) noexcept
{
    if (!spec)
        return;
    if (backend != Renderers::OPENGL && backend != Renderers::EGL)
        backend = Renderers::OPENGL;
    RendererContextSettings s{};
    s.backend = backend;
    s.gl_major = gl_major > 0 ? gl_major : 3;
    s.gl_minor = gl_minor >= 0 ? gl_minor : 3;
    s.gl_core_profile = core_profile ? 1 : 0;
    s.gl_forward_compatible = forward_compatible ? 1 : 0;
    s.double_buffer = double_buffer ? 1 : 0;
    s.color_bits_override = 0;
    s.depth_bits_override = 0;
    s.stencil_bits_override = 0;
    s.msaa_samples_override = 0;
    s.vsync = vsync ? 1 : 0;
    s.minimum_honkord_gl_api_level = 0;
    s.requested_gpu_device = 0;
    *spec = s;
}

} // namespace HonkordGL::Graphics
