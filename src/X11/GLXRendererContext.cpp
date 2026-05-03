/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — GLX implementation for RendererContext (Linux + X11)
    * Copyright (C) 2026 Honkord
    *
    * Link: -lGL -lX11 (-lEGL when linking EGLRendererContextX11.cpp)
    */

#include <HonkordGL/RendererContext.h>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <GL/gl.h>
#include <GL/glx.h>

#include <X11/Xlib.h>

#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace HonkordGL::Graphics::Internal {
int AttachRendererContextWaylandEGL(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept;
void DetachRendererContextWaylandEGL(ApplicationContextSettings& app) noexcept;
int RendererContextMakeCurrentWaylandEGL(ApplicationContextSettings& app) noexcept;
void RendererContextSwapBuffersWaylandEGL(ApplicationContextSettings& app) noexcept;

int AttachRendererContextX11EGL(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept;
void DetachRendererContextX11EGL(ApplicationContextSettings& app) noexcept;
int RendererContextMakeCurrentX11EGL(ApplicationContextSettings& app) noexcept;
void RendererContextSwapBuffersX11EGL(ApplicationContextSettings& app) noexcept;

int AttachRendererContextVulkan(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept;
void DetachRendererContextVulkan(ApplicationContextSettings& app) noexcept;
int RendererContextMakeCurrentVulkan(ApplicationContextSettings& app) noexcept;
void RendererContextSwapBuffersVulkan(ApplicationContextSettings& app) noexcept;
} // namespace HonkordGL::Graphics::Internal

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef GLX_CONTEXT_FLAGS_ARB
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#endif
#ifndef GLX_CONTEXT_PROFILE_MASK_ARB
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef GLX_CONTEXT_CORE_PROFILE_BIT_ARB
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif
#ifndef GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#endif
#ifndef GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

namespace HonkordGL::Graphics {

namespace {

inline bool NativePlatformUsesX11Glx(NativePlatform p) noexcept
{
    return NativePlatformUsesX11ClientHandles(p);
}

using PFNGLXCREATECONTEXTATTRIBSARBPROC = GLXContext (*)(Display *, GLXFBConfig, GLXContext, int, const int *);
using PFNGLXSWAPINTERVALEXTPROC = void (*)(Display *, GLXDrawable, int);

inline Display * GetDisplay(ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<Display *>(app.device_context);
}
inline ::Window GetXWindow(const ApplicationContextSettings& app) noexcept
{
    return static_cast<::Window>(reinterpret_cast<std::uintptr_t>(app.window_handle));
}
inline GLXContext GetGLXContext(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<GLXContext>(app.device);
}
void ClearRendererHandles(ApplicationContextSettings& app) noexcept
{
    app.device = nullptr;
    app.surface = nullptr;
    app.egl_display = nullptr;
    app.renderer_private = nullptr;
    app.active_renderer = 0;
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
void AppendColorBits(std::vector<int>& attribs, int colorBits) noexcept
{
    if (colorBits <= 0)
        return;
    if (colorBits >= 32) {
        attribs.push_back(GLX_RED_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_GREEN_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_BLUE_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_ALPHA_SIZE);
        attribs.push_back(8);
    } else if (colorBits >= 24) {
        attribs.push_back(GLX_RED_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_GREEN_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_BLUE_SIZE);
        attribs.push_back(8);
        attribs.push_back(GLX_ALPHA_SIZE);
        attribs.push_back(0);
    }
}
GLXFBConfig ChooseMatchingFbConfig(
    Display * dpy,
    int screen,
    ::Window win,
    int colorBits,
    int depthBits,
    int stencilBits,
    int msaaSamples,
    int wantDoubleBuffer) noexcept
{
    XWindowAttributes wa{};
    if (XGetWindowAttributes(dpy, win, &wa) == 0)
        return nullptr;
    const VisualID windowVisual = XVisualIDFromVisual(wa.visual);

    const int samples = msaaSamples > 1 ? msaaSamples : 0;

    std::vector<int> attribs;
    attribs.reserve(32);
    attribs.push_back(GLX_X_RENDERABLE);
    attribs.push_back(True);
    attribs.push_back(GLX_DRAWABLE_TYPE);
    attribs.push_back(GLX_WINDOW_BIT);
    attribs.push_back(GLX_RENDER_TYPE);
    attribs.push_back(GLX_RGBA_BIT);
    attribs.push_back(GLX_X_VISUAL_TYPE);
    attribs.push_back(GLX_TRUE_COLOR);
    attribs.push_back(GLX_DOUBLEBUFFER);
    attribs.push_back(wantDoubleBuffer ? True : False);
    AppendColorBits(attribs, colorBits);
    attribs.push_back(GLX_DEPTH_SIZE);
    attribs.push_back(depthBits);
    attribs.push_back(GLX_STENCIL_SIZE);
    attribs.push_back(stencilBits);
    if (samples > 0) {
        attribs.push_back(GLX_SAMPLE_BUFFERS);
        attribs.push_back(1);
        attribs.push_back(GLX_SAMPLES);
        attribs.push_back(samples);
    }
    attribs.push_back(None);

    int ncfg = 0;
    GLXFBConfig* cfgs = glXChooseFBConfig(dpy, screen, attribs.data(), &ncfg);
    if (!cfgs || ncfg <= 0) {
        if (cfgs)
            XFree(cfgs);
        return nullptr;
    }

    GLXFBConfig chosen = nullptr;
    for (int i = 0; i < ncfg; ++i) {
        XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, cfgs[i]);
        if (!vi)
            continue;
        const bool match = static_cast<VisualID>(vi->visualid) == windowVisual;
        XFree(vi);
        if (match) {
            chosen = cfgs[i];
            break;
        }
    }

    if (!chosen)
        chosen = cfgs[0];

    XFree(cfgs);
    return chosen;
}

} // namespace

int AttachRendererContext(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    {
        const int vr = ValidateRendererDeviceRequest(spec);
        if (vr != static_cast<int>(RendererContextResult::OK))
            return vr;
    }
    if (app.device != nullptr || app.active_renderer != 0)
        DetachRendererContext(app);

    if (spec.backend == Renderers::VULKAN)
        return Internal::AttachRendererContextVulkan(app, spec);

    if (spec.backend != Renderers::OPENGL && spec.backend != Renderers::EGL) {
        SetInternalApiError("AttachRendererContext: unsupported renderer backend.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::Wayland)
        return Internal::AttachRendererContextWaylandEGL(app, spec);

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::DRM) {
        SetInternalApiError("AttachRendererContext: DRM/KMS needs EGL+GBM, not GLX in this build.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }

    if (NativePlatformUsesX11Glx(static_cast<NativePlatform>(app.native_platform))
        && spec.backend == Renderers::EGL)
        return Internal::AttachRendererContextX11EGL(app, spec);

    if (spec.backend != Renderers::OPENGL) {
        SetInternalApiError("AttachRendererContext: Renderers::EGL is only for X11 or Wayland here.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }

    Display * const dpy = GetDisplay(app);
    if (!dpy) {
        SetInternalApiError("AttachRendererContext: no X11 Display (device_context).");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    const ::Window win = GetXWindow(app);
    if (!win) {
        SetInternalApiError("AttachRendererContext: no X window handle.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    const int screen = DefaultScreen(dpy);
    const int colorBits = EffectiveColorBits(app, spec);
    const int depthBits = EffectiveDepth(app, spec);
    const int stencilBits = EffectiveStencil(app, spec);
    const int msaa = EffectiveMsaa(app, spec);

    GLXFBConfig fbc = ChooseMatchingFbConfig(
        dpy,
        screen,
        win,
        colorBits,
        depthBits,
        stencilBits,
        msaa,
        spec.double_buffer ? 1 : 0);
    if (!fbc) {
        SetInternalApiError("AttachRendererContext: GLX pixel format / FBConfig failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    auto createAttribs = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
        glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));

    GLXContext share = nullptr;
    GLXContext glxctx = nullptr;

    if (createAttribs) {
        const int maj = spec.gl_major > 0 ? spec.gl_major : 3;
        const int min = spec.gl_minor > 0 ? spec.gl_minor : 3;
        int flags = 0;
        if (spec.gl_forward_compatible && spec.gl_core_profile)
            flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

        const int profileMask = spec.gl_core_profile ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
                                                     : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

        const int ctxAttr[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB,
            maj,
            GLX_CONTEXT_MINOR_VERSION_ARB,
            min,
            GLX_CONTEXT_PROFILE_MASK_ARB,
            profileMask,
            GLX_CONTEXT_FLAGS_ARB,
            flags,
            None,
        };

        glxctx = createAttribs(dpy, fbc, share, True, ctxAttr);
    }

    if (!glxctx)
        glxctx = glXCreateNewContext(dpy, fbc, GLX_RGBA_TYPE, share, True);

    if (!glxctx) {
        SetInternalApiError("AttachRendererContext: GLX context creation failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (glXMakeCurrent(dpy, win, glxctx) == False) {
        glXDestroyContext(dpy, glxctx);
        SetInternalApiError("AttachRendererContext: glXMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    app.device = reinterpret_cast<HonkordGL_GW_Handle>(glxctx);
    app.color_bits = colorBits;
    app.depth_bits = depthBits;
    app.stencil_bits = stencilBits;
    app.msaa_samples = msaa > 0 ? msaa : 1;
    app.vsync_enabled = spec.vsync;
    if (spec.vsync) {
        auto swapInterval = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(
            glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXSwapIntervalEXT")));
        if (swapInterval)
            swapInterval(dpy, win, 1);
    }

    app.active_renderer = static_cast<int>(Renderers::OPENGL);
    return static_cast<int>(RendererContextResult::OK);
}
void DetachRendererContext(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::VULKAN)) {
        Internal::DetachRendererContextVulkan(app);
        return;
    }

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::Wayland) {
        Internal::DetachRendererContextWaylandEGL(app);
        return;
    }

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::DRM) {
        ClearRendererHandles(app);
        return;
    }

    if (NativePlatformUsesX11Glx(static_cast<NativePlatform>(app.native_platform)) && app.egl_display) {
        Internal::DetachRendererContextX11EGL(app);
        return;
    }

    Display * const dpy = GetDisplay(app);
    GLXContext const glxctx = GetGLXContext(app);
    if (!dpy || !glxctx) {
        ClearRendererHandles(app);
        return;
    }

    glXMakeCurrent(dpy, None, nullptr);
    glXDestroyContext(dpy, glxctx);
    ClearRendererHandles(app);
}
int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::VULKAN))
        return Internal::RendererContextMakeCurrentVulkan(app);

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::Wayland)
        return Internal::RendererContextMakeCurrentWaylandEGL(app);

    if (NativePlatformUsesX11Glx(static_cast<NativePlatform>(app.native_platform)) && app.egl_display)
        return Internal::RendererContextMakeCurrentX11EGL(app);

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::DRM) {
        SetInternalApiError("RendererContextMakeCurrent: DRM/KMS needs EGL+GBM, not GLX.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }

    Display * const dpy = GetDisplay(app);
    const ::Window win = GetXWindow(app);
    GLXContext const glxctx = GetGLXContext(app);
    if (!dpy || !win || !glxctx) {
        SetInternalApiError("RendererContextMakeCurrent: no GLX context or window.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }

    if (glXMakeCurrent(dpy, win, glxctx) == False) {
        SetInternalApiError("RendererContextMakeCurrent: glXMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::VULKAN)) {
        Internal::RendererContextSwapBuffersVulkan(app);
        return;
    }

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::Wayland) {
        Internal::RendererContextSwapBuffersWaylandEGL(app);
        return;
    }

    if (static_cast<NativePlatform>(app.native_platform) == NativePlatform::DRM)
        return;

    if (NativePlatformUsesX11Glx(static_cast<NativePlatform>(app.native_platform)) && app.egl_display) {
        Internal::RendererContextSwapBuffersX11EGL(app);
        return;
    }

    Display * const dpy = GetDisplay(app);
    const ::Window win = GetXWindow(app);
    if (!dpy || !win)
        return;
    glXSwapBuffers(dpy, win);
}

} // namespace HonkordGL::Graphics

#elif defined(__APPLE__)
/* Implemented in src/Cocoa/NSOpenGLRendererContext.mm */

#elif defined(_WIN32)
/* Implemented in src/Win32/WGLRendererContext.cpp */

#elif defined(__ANDROID__)
/* Implemented in src/Android/EGLRendererContextAndroid.cpp */

#else /* unknown host */

namespace HonkordGL::Graphics {

int AttachRendererContext(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    (void)app;
    (void)spec;
    return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
}
void DetachRendererContext(ApplicationContextSettings& app) noexcept
{
    app.device = nullptr;
    app.surface = nullptr;
    app.egl_display = nullptr;
    app.renderer_private = nullptr;
    app.active_renderer = 0;
}
int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept
{
    (void)app;
    return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
}
void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept
{
    (void)app;
}

} // namespace HonkordGL::Graphics

#endif