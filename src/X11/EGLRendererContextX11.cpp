/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — EGL + X11 for RendererContext (Linux)
 * Copyright (C) 2026 Honkord
 *
 * Link: -lEGL -lX11 -lGL
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <X11/Xlib.h>

#include <cstdint>
#include <cstring>

#ifndef EGL_PLATFORM_X11_KHR
#define EGL_PLATFORM_X11_KHR 0x31D5
#endif
#ifndef EGL_CONTEXT_FLAGS_KHR
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#endif
#ifndef EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#endif

namespace HonkordGL::Graphics::Internal {

namespace {

using PFN_eglGetPlatformDisplay = EGLDisplay (*)(EGLenum, void *, const EGLint *);
using PFN_eglCreatePlatformWindowSurface = EGLSurface (*)(EGLDisplay, EGLConfig, void*, const EGLint *);

inline Display * GetDisplay(ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<Display *>(app.device_context);
}

inline ::Window GetXWindow(const ApplicationContextSettings& app) noexcept
{
    return static_cast<::Window>(reinterpret_cast<std::uintptr_t>(app.window_handle));
}
EGLDisplay GetEglDisplayForX11(Display * x11dpy) noexcept
{
    auto getPlatformDisplay =
        reinterpret_cast<PFN_eglGetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplay"));
    if (getPlatformDisplay)
        return getPlatformDisplay(EGL_PLATFORM_X11_KHR, x11dpy, nullptr);

    auto getPlatformDisplayEXT =
        reinterpret_cast<PFN_eglGetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (getPlatformDisplayEXT)
        return getPlatformDisplayEXT(EGL_PLATFORM_X11_KHR, x11dpy, nullptr);

    return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(x11dpy));
}
EGLSurface CreateX11WindowSurface(EGLDisplay edpy, EGLConfig cfg, ::Window * xwin) noexcept
{
    auto createPws = reinterpret_cast<PFN_eglCreatePlatformWindowSurface>(
        eglGetProcAddress("eglCreatePlatformWindowSurface"));
    if (createPws)
        return createPws(edpy, cfg, xwin, nullptr);
    auto createPwsEXT = reinterpret_cast<PFN_eglCreatePlatformWindowSurface>(
        eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT"));
    if (createPwsEXT)
        return createPwsEXT(edpy, cfg, xwin, nullptr);
    return eglCreateWindowSurface(edpy, cfg, static_cast<EGLNativeWindowType>(*xwin), nullptr);
}
int EffectiveDepth(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.depth_bits_override > 0)
        return spec.depth_bits_override;
    if (app.depth_bits > 0)
        return app.depth_bits;
    return 24;
}
int EffectiveStencil(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.stencil_bits_override > 0)
        return spec.stencil_bits_override;
    if (app.stencil_bits > 0)
        return app.stencil_bits;
    return 8;
}
int EffectiveMsaa(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.msaa_samples_override > 0)
        return spec.msaa_samples_override;
    if (app.msaa_samples > 0)
        return app.msaa_samples;
    return 1;
}
int EffectiveColorBits(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.color_bits_override > 0)
        return spec.color_bits_override;
    if (app.color_bits > 0)
        return app.color_bits;
    return 32;
}

} // namespace

int AttachRendererContextX11EGL(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.backend != Renderers::EGL) {
        SetInternalApiError("AttachRendererContext: X11 EGL path expects Renderers::EGL.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }
    if (app.device || app.egl_display) {
        SetInternalApiError("AttachRendererContext: renderer already attached.");
        return static_cast<int>(RendererContextResult::ALREADY_ATTACHED);
    }

    Display * const x11dpy = GetDisplay(app);
    if (!x11dpy) {
        SetInternalApiError("AttachRendererContext: no X11 Display (device_context).");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    ::Window const xwin = GetXWindow(app);
    if (!xwin) {
        SetInternalApiError("AttachRendererContext: no X window handle.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    EGLDisplay const edpy = GetEglDisplayForX11(x11dpy);
    if (edpy == EGL_NO_DISPLAY) {
        SetInternalApiError("AttachRendererContext: eglGetPlatformDisplay (X11) failed.");
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

    const int colorBits = EffectiveColorBits(app, spec);
    const int depthBits = EffectiveDepth(app, spec);
    const int stencilBits = EffectiveStencil(app, spec);
    const int msaa = EffectiveMsaa(app, spec);

    EGLint cfg_attribs[32];
    int ci = 0;
    cfg_attribs[ci++] = EGL_SURFACE_TYPE;
    cfg_attribs[ci++] = EGL_WINDOW_BIT;
    cfg_attribs[ci++] = EGL_RENDERABLE_TYPE;
    cfg_attribs[ci++] = EGL_OPENGL_BIT;
    cfg_attribs[ci++] = EGL_RED_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(colorBits >= 32 ? 8 : 8);
    cfg_attribs[ci++] = EGL_GREEN_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(colorBits >= 32 ? 8 : 8);
    cfg_attribs[ci++] = EGL_BLUE_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(colorBits >= 32 ? 8 : 8);
    cfg_attribs[ci++] = EGL_ALPHA_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(colorBits >= 32 ? 8 : 0);
    cfg_attribs[ci++] = EGL_DEPTH_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(depthBits);
    cfg_attribs[ci++] = EGL_STENCIL_SIZE;
    cfg_attribs[ci++] = static_cast<EGLint>(stencilBits);
    (void)spec.double_buffer;
    if (msaa > 1) {
        cfg_attribs[ci++] = EGL_SAMPLE_BUFFERS;
        cfg_attribs[ci++] = 1;
        cfg_attribs[ci++] = EGL_SAMPLES;
        cfg_attribs[ci++] = static_cast<EGLint>(msaa);
    }
    cfg_attribs[ci++] = EGL_NONE;

    EGLint ncfg = 0;
    EGLConfig cfg{};
    if (!eglChooseConfig(edpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
        SetInternalApiError("AttachRendererContext: eglChooseConfig (X11) failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    ::Window winMut = xwin;
    EGLSurface const surf = CreateX11WindowSurface(edpy, cfg, &winMut);
    if (surf == EGL_NO_SURFACE) {
        SetInternalApiError("AttachRendererContext: eglCreatePlatformWindowSurface (X11) failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    const int maj = spec.gl_major > 0 ? spec.gl_major : 3;
    const int min = spec.gl_minor >= 0 ? spec.gl_minor : 3;

    EGLint ctx_attr[20];
    int vi = 0;
    ctx_attr[vi++] = EGL_CONTEXT_MAJOR_VERSION;
    ctx_attr[vi++] = maj;
    ctx_attr[vi++] = EGL_CONTEXT_MINOR_VERSION;
    ctx_attr[vi++] = min;
    ctx_attr[vi++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
    ctx_attr[vi++] = spec.gl_core_profile ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
                                          : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
    if (spec.gl_forward_compatible && spec.gl_core_profile) {
        ctx_attr[vi++] = EGL_CONTEXT_FLAGS_KHR;
        ctx_attr[vi++] = EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
    }
    ctx_attr[vi++] = EGL_NONE;

    EGLContext eglctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (eglctx == EGL_NO_CONTEXT)
        eglctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, nullptr);
    if (eglctx == EGL_NO_CONTEXT) {
        eglDestroySurface(edpy, surf);
        SetInternalApiError("AttachRendererContext: eglCreateContext (X11) failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!eglMakeCurrent(edpy, surf, surf, eglctx)) {
        eglDestroyContext(edpy, eglctx);
        eglDestroySurface(edpy, surf);
        SetInternalApiError("AttachRendererContext: eglMakeCurrent (X11) failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    app.egl_display = reinterpret_cast<HonkordGL_GW_Handle>(edpy);
    app.device = reinterpret_cast<HonkordGL_GW_Handle>(eglctx);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(surf);
    app.color_bits = colorBits >= 32 ? 32 : 24;
    app.depth_bits = depthBits;
    app.stencil_bits = stencilBits;
    app.msaa_samples = msaa > 0 ? msaa : 1;
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
void DetachRendererContextX11EGL(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = static_cast<EGLDisplay>(app.egl_display);
    EGLContext const ctx = static_cast<EGLContext>(app.device);
    EGLSurface const surf = static_cast<EGLSurface>(app.surface);

    if (edpy == EGL_NO_DISPLAY) {
        app.device = nullptr;
        app.surface = nullptr;
        app.egl_display = nullptr;
        app.active_renderer = 0;
        return;
    }

    eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (ctx != EGL_NO_CONTEXT)
        eglDestroyContext(edpy, ctx);
    if (surf != EGL_NO_SURFACE)
        eglDestroySurface(edpy, surf);

    app.device = nullptr;
    app.surface = nullptr;
    app.egl_display = nullptr;
    app.active_renderer = 0;
}
int RendererContextMakeCurrentX11EGL(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = static_cast<EGLDisplay>(app.egl_display);
    EGLSurface const surf = static_cast<EGLSurface>(app.surface);
    EGLContext const ctx = static_cast<EGLContext>(app.device);
    if (edpy == EGL_NO_DISPLAY || surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT) {
        SetInternalApiError("RendererContextMakeCurrent: no X11 EGL context.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }
    if (!eglMakeCurrent(edpy, surf, surf, ctx)) {
        SetInternalApiError("RendererContextMakeCurrent: eglMakeCurrent (X11) failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffersX11EGL(ApplicationContextSettings& app) noexcept
{
    EGLDisplay const edpy = static_cast<EGLDisplay>(app.egl_display);
    EGLSurface const surf = static_cast<EGLSurface>(app.surface);
    if (edpy == EGL_NO_DISPLAY || surf == EGL_NO_SURFACE)
        return;
    eglSwapBuffers(edpy, surf);
}

} // namespace HonkordGL::Graphics::Internal

#endif // HONKORDGL_PLATFORM_USES_X11_SOURCES