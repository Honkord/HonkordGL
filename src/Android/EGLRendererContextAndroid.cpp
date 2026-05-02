/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — EGL + OpenGL ES for RendererContext (Android)
 * Copyright (C) 2026 Honkord
 *
 * Link: -lEGL -lGLESv3 (or -lGLESv2)
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#include "AndroidWindowContext.hpp"

#if defined(__ANDROID__)

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <android/native_window.h>

namespace HonkordGL::Graphics {

namespace {

inline EGLDisplay GetEglDisplayApp(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<EGLDisplay>(app.egl_display);
}
inline EGLContext GetEglContextApp(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<EGLContext>(app.device);
}
inline EGLSurface GetEglSurfaceApp(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<EGLSurface>(app.surface);
}

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

bool PickEglConfig(EGLDisplay dpy, EGLint renderable_bits, EGLConfig * out) noexcept
{
    const EGLint cfg_attrs[] = {
        EGL_RENDERABLE_TYPE,
        renderable_bits,
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_RED_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_DEPTH_SIZE,
        24,
        EGL_STENCIL_SIZE,
        8,
        EGL_NONE,
    };
    EGLint n = 0;
    return eglChooseConfig(dpy, cfg_attrs, out, 1, &n) == EGL_TRUE && n > 0;
}

int EffectiveColorBitsAndroid(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.color_bits_override > 0)
        return spec.color_bits_override;
    if (app.color_bits > 0)
        return app.color_bits;
    return 32;
}

} // namespace

int AttachRendererContext(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.backend != Renderers::OPENGL) {
        SetInternalApiError("AttachRendererContext: Android uses OpenGL ES via EGL; use Renderers::OPENGL.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Android) {
        SetInternalApiError("AttachRendererContext: native platform is not Android.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }
    if (app.device != nullptr || app.egl_display != nullptr || app.active_renderer != 0)
        DetachRendererContext(app);

    auto * const st = reinterpret_cast<Android::Internal::AndroidWindowState *>(app.window_handle);
    if (!st || !st->native_window) {
        SetInternalApiError("AttachRendererContext: no ANativeWindow (create window after APP_CMD_INIT_WINDOW).");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    EGLDisplay const edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (edpy == EGL_NO_DISPLAY) {
        SetInternalApiError("AttachRendererContext: eglGetDisplay failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }
    EGLint maj = 0;
    EGLint min = 0;
    if (!eglInitialize(edpy, &maj, &min)) {
        SetInternalApiError("AttachRendererContext: eglInitialize failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        eglTerminate(edpy);
        SetInternalApiError("AttachRendererContext: eglBindAPI(EGL_OPENGL_ES_API) failed.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }

    const bool want_es3 = (spec.gl_major >= 3);
    EGLConfig cfg{};
    bool have_es3 = PickEglConfig(edpy, EGL_OPENGL_ES3_BIT_KHR, &cfg);
    if (!have_es3 && !PickEglConfig(edpy, EGL_OPENGL_ES2_BIT, &cfg)) {
        eglTerminate(edpy);
        SetInternalApiError("AttachRendererContext: eglChooseConfig failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    const EGLint surf_attrs[] = { EGL_NONE };
    EGLSurface const surf = eglCreateWindowSurface(edpy, cfg, st->native_window, surf_attrs);
    if (surf == EGL_NO_SURFACE) {
        eglTerminate(edpy);
        SetInternalApiError("AttachRendererContext: eglCreateWindowSurface failed.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }

    const int es_version = (want_es3 && have_es3) ? 3 : 2;
    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        es_version,
        EGL_NONE,
    };

    EGLContext const ectx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
    if (ectx == EGL_NO_CONTEXT) {
        eglDestroySurface(edpy, surf);
        eglTerminate(edpy);
        SetInternalApiError("AttachRendererContext: eglCreateContext (ES) failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!eglMakeCurrent(edpy, surf, surf, ectx)) {
        eglDestroyContext(edpy, ectx);
        eglDestroySurface(edpy, surf);
        eglTerminate(edpy);
        SetInternalApiError("AttachRendererContext: eglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    app.egl_display = reinterpret_cast<HonkordGL_GW_Handle>(edpy);
    app.device = reinterpret_cast<HonkordGL_GW_Handle>(ectx);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(surf);
    app.color_bits = EffectiveColorBitsAndroid(app, spec);
    app.depth_bits = spec.depth_bits_override > 0 ? spec.depth_bits_override : (app.depth_bits > 0 ? app.depth_bits : 24);
    app.stencil_bits = spec.stencil_bits_override > 0 ? spec.stencil_bits_override : (app.stencil_bits > 0 ? app.stencil_bits : 8);
    app.msaa_samples = spec.msaa_samples_override > 0 ? spec.msaa_samples_override : (app.msaa_samples > 0 ? app.msaa_samples : 1);
    app.vsync_enabled = spec.vsync;
    if (spec.vsync) {
        typedef EGLBoolean (*PFNEGLSWAPINTERVALPROC)(EGLDisplay, EGLSurface, EGLint);
        auto swapInterval = reinterpret_cast<PFNEGLSWAPINTERVALPROC>(eglGetProcAddress("eglSwapInterval"));
        if (swapInterval)
            swapInterval(edpy, surf, 1);
    }

    return static_cast<int>(RendererContextResult::OK);
}

void DetachRendererContext(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = GetEglDisplayApp(app);
    if (edpy == EGL_NO_DISPLAY) {
        app.device = nullptr;
        app.surface = nullptr;
        app.egl_display = nullptr;
        return;
    }
    EGLContext const ctx = GetEglContextApp(app);
    EGLSurface const surf = GetEglSurfaceApp(app);

    eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (ctx != EGL_NO_CONTEXT)
        eglDestroyContext(edpy, ctx);
    if (surf != EGL_NO_SURFACE)
        eglDestroySurface(edpy, surf);
    eglTerminate(edpy);

    app.device = nullptr;
    app.surface = nullptr;
    app.egl_display = nullptr;
}

int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = GetEglDisplayApp(app);
    if (edpy == EGL_NO_DISPLAY) {
        SetInternalApiError("RendererContextMakeCurrent: no EGL display.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }
    EGLSurface const surf = GetEglSurfaceApp(app);
    EGLContext const ctx = GetEglContextApp(app);
    if (!eglMakeCurrent(edpy, surf, surf, ctx)) {
        SetInternalApiError("RendererContextMakeCurrent: eglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }
    return static_cast<int>(RendererContextResult::OK);
}

void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = GetEglDisplayApp(app);
    EGLSurface const surf = GetEglSurfaceApp(app);
    if (edpy == EGL_NO_DISPLAY || surf == EGL_NO_SURFACE)
        return;
    (void)eglSwapBuffers(edpy, surf);
}

} // namespace HonkordGL::Graphics

#endif
