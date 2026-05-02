/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — EGL + Wayland for RendererContext (Linux)
 * Copyright (C) 2026 Honkord
 *
 * Link: -lEGL -lwayland-egl -lGL
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#if HONKORDGL_PLATFORM_LINUX_DESKTOP

#include "WaylandWindowContext.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdint>
#include <cstring>

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

namespace HonkordGL::Graphics {

namespace {

using PFN_eglGetPlatformDisplay = EGLDisplay (*)(EGLenum, void*, const EGLint*);
using PFN_eglCreatePlatformWindowSurface = EGLSurface (*)(EGLDisplay, EGLConfig, void*, const EGLint*);

inline HonkordGL::Graphics::Wayland::Internal::WaylandWindowContext* GetWctx(ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<HonkordGL::Graphics::Wayland::Internal::WaylandWindowContext*>(app.window_handle);
}

EGLDisplay GetEglDisplay(wl_display* dpy) noexcept
{
    auto getPlatformDisplay =
        reinterpret_cast<PFN_eglGetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplay"));
    if (getPlatformDisplay)
        return getPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, dpy, nullptr);

    auto getPlatformDisplayEXT =
        reinterpret_cast<PFN_eglGetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (getPlatformDisplayEXT)
        return getPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR, dpy, nullptr);

    return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(dpy));
}

EGLSurface CreateWindowSurface(EGLDisplay edpy, EGLConfig cfg, wl_egl_window* egl_win) noexcept
{
    auto createPws = reinterpret_cast<PFN_eglCreatePlatformWindowSurface>(
        eglGetProcAddress("eglCreatePlatformWindowSurface"));
    if (createPws)
        return createPws(edpy, cfg, egl_win, nullptr);
    auto createPwsEXT = reinterpret_cast<PFN_eglCreatePlatformWindowSurface>(
        eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    if (createPwsEXT)
        return createPwsEXT(edpy, cfg, egl_win, nullptr);
    return eglCreateWindowSurface(edpy, cfg, reinterpret_cast<EGLNativeWindowType>(egl_win), nullptr);
}

void ClearEglFromWindow(HonkordGL::Graphics::Wayland::Internal::WaylandWindowContext* w) noexcept
{
    if (!w)
        return;
    w->egl_display = nullptr;
    w->egl_context = nullptr;
    w->egl_surface = nullptr;
}

} // namespace

namespace Internal {

int AttachRendererContextWaylandEGL(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.backend != Renderers::OPENGL && spec.backend != Renderers::EGL) {
        SetInternalApiError("AttachRendererContext: unsupported renderer backend.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }
    if (app.device) {
        SetInternalApiError("AttachRendererContext: renderer already attached.");
        return static_cast<int>(RendererContextResult::ALREADY_ATTACHED);
    }
    auto* wctx = GetWctx(app);
    if (!wctx || !wctx->display || !wctx->surface) {
        SetInternalApiError("AttachRendererContext: no Wayland window.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    EGLDisplay const edpy = GetEglDisplay(wctx->display);
    if (edpy == EGL_NO_DISPLAY) {
        SetInternalApiError("AttachRendererContext: eglGetPlatformDisplay failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    if (!eglInitialize(edpy, nullptr, nullptr)) {
        SetInternalApiError("AttachRendererContext: eglInitialize failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    if (!eglBindAPI(EGL_OPENGL_API)) {
        SetInternalApiError("AttachRendererContext: eglBindAPI(EGL_OPENGL_API) failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    EGLint ncfg = 0;
    const EGLint cfg_attribs[] = {
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_DEPTH_SIZE,
        24,
        EGL_STENCIL_SIZE,
        8,
        EGL_NONE,
    };
    EGLConfig cfg{};
    if (!eglChooseConfig(edpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
        SetInternalApiError("AttachRendererContext: eglChooseConfig failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    if (!wctx->egl_window)
        wctx->egl_window = wl_egl_window_create(wctx->surface, wctx->width > 0 ? wctx->width : 640,
            wctx->height > 0 ? wctx->height : 480);
    if (!wctx->egl_window) {
        SetInternalApiError("AttachRendererContext: wl_egl_window_create failed.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    EGLSurface const surf = CreateWindowSurface(edpy, cfg, wctx->egl_window);
    if (surf == EGL_NO_SURFACE) {
        SetInternalApiError("AttachRendererContext: eglCreateWindowSurface failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    const int maj = spec.gl_major > 0 ? spec.gl_major : 3;
    const int min = spec.gl_minor > 0 ? spec.gl_minor : 3;

    EGLint ctx_attr[] = {
        EGL_CONTEXT_MAJOR_VERSION,
        maj,
        EGL_CONTEXT_MINOR_VERSION,
        min,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE,
    };

    EGLContext eglctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (eglctx == EGL_NO_CONTEXT)
        eglctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, nullptr);
    if (eglctx == EGL_NO_CONTEXT) {
        eglDestroySurface(edpy, surf);
        SetInternalApiError("AttachRendererContext: eglCreateContext failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!eglMakeCurrent(edpy, surf, surf, eglctx)) {
        eglDestroyContext(edpy, eglctx);
        eglDestroySurface(edpy, surf);
        SetInternalApiError("AttachRendererContext: eglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    wctx->egl_display = edpy;
    wctx->egl_context = eglctx;
    wctx->egl_surface = surf;

    app.egl_display = reinterpret_cast<HonkordGL_GW_Handle>(edpy);
    app.device = reinterpret_cast<HonkordGL_GW_Handle>(eglctx);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(surf);
    app.device_context = reinterpret_cast<HonkordGL_GW_Handle>(wctx->display);
    app.color_bits = 32;
    app.depth_bits = 24;
    app.stencil_bits = 8;
    app.msaa_samples = 1;
    app.vsync_enabled = spec.vsync;

    if (spec.vsync) {
        typedef EGLBoolean (*PFNEGLSWAPINTERVALPROC)(EGLDisplay, EGLSurface, EGLint);
        auto swapInterval =
            reinterpret_cast<PFNEGLSWAPINTERVALPROC>(eglGetProcAddress("eglSwapInterval"));
        if (swapInterval)
            swapInterval(edpy, surf, 1);
    }

    app.active_renderer = static_cast<int>(Renderers::OPENGL);
    return static_cast<int>(RendererContextResult::OK);
}

void DetachRendererContextWaylandEGL(ApplicationContextSettings& app) noexcept
{
    auto* wctx = GetWctx(app);
    if (!wctx || !wctx->egl_display) {
        app.device = nullptr;
        app.surface = nullptr;
        app.egl_display = nullptr;
        app.active_renderer = 0;
        ClearEglFromWindow(wctx);
        return;
    }
    EGLDisplay const edpy = static_cast<EGLDisplay>(wctx->egl_display);
    EGLContext const ctx = static_cast<EGLContext>(app.device);
    EGLSurface const surf = static_cast<EGLSurface>(app.surface);

    eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (ctx != EGL_NO_CONTEXT)
        eglDestroyContext(edpy, ctx);
    if (surf != EGL_NO_SURFACE)
        eglDestroySurface(edpy, surf);
    if (wctx->egl_window) {
        wl_egl_window_destroy(wctx->egl_window);
        wctx->egl_window = nullptr;
    }
    ClearEglFromWindow(wctx);
    app.device = nullptr;
    app.surface = nullptr;
    app.egl_display = nullptr;
    app.active_renderer = 0;
}

int RendererContextMakeCurrentWaylandEGL(ApplicationContextSettings& app) noexcept
{
    auto* wctx = GetWctx(app);
    EGLDisplay const edpy = static_cast<EGLDisplay>(app.egl_display);
    if (edpy == EGL_NO_DISPLAY) {
        SetInternalApiError("RendererContextMakeCurrent: no EGL display.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }
    if (!wctx) {
        SetInternalApiError("RendererContextMakeCurrent: no Wayland window.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }
    EGLSurface const surf = static_cast<EGLSurface>(app.surface);
    EGLContext const ctx = static_cast<EGLContext>(app.device);
    if (!eglMakeCurrent(edpy, surf, surf, ctx)) {
        SetInternalApiError("RendererContextMakeCurrent: eglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }
    return static_cast<int>(RendererContextResult::OK);
}

void RendererContextSwapBuffersWaylandEGL(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = static_cast<EGLDisplay>(app.egl_display);
    if (edpy == EGL_NO_DISPLAY || !app.surface)
        return;
    eglSwapBuffers(edpy, static_cast<EGLSurface>(app.surface));
}

} // namespace Internal

} // namespace HonkordGL::Graphics

#endif
