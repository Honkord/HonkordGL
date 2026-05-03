/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — WGL implementation for RendererContext (Windows)
 * Copyright (C) 2026 Honkord
 *
 * Link: opengl32.lib
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#if defined(_WIN32)

#include "../internal/D3D11RendererContextWin32.hpp"
#include "Win32WindowContext.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_FLAGS_ARB
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif
#ifndef WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif
#ifndef WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#endif

namespace HonkordGL::Graphics {

namespace {

using Win32::Internal::Win32WindowState;

typedef HGLRC(WINAPI * PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL(WINAPI * PFN_wglSwapIntervalEXT)(int);

HWND GetHwnd(const ApplicationContextSettings& app) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32)
        return nullptr;
    auto * const st = reinterpret_cast<Win32WindowState *>(app.window_handle);
    return st ? st->hwnd : nullptr;
}
inline HGLRC GetHglrc(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<HGLRC>(app.device);
}
inline HDC GetHdc(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<HDC>(app.surface);
}
void ClearRendererHandles(ApplicationContextSettings& app) noexcept
{
    app.device = nullptr;
    app.device_context = nullptr;
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
int EffectiveColorBits(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.color_bits_override > 0)
        return spec.color_bits_override;
    if (app.color_bits > 0)
        return app.color_bits;
    return 32;
}
int EffectiveMsaa(const ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (spec.msaa_samples_override > 0)
        return spec.msaa_samples_override;
    if (app.msaa_samples > 0)
        return app.msaa_samples;
    return 1;
}

} // namespace

int AttachRendererContext(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32) {
        SetInternalApiError("AttachRendererContext: native platform is not Win32.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }

    {
        const int vr = ValidateRendererDeviceRequest(spec);
        if (vr != static_cast<int>(RendererContextResult::OK))
            return vr;
    }

    if (app.device != nullptr || app.active_renderer != 0)
        DetachRendererContext(app);

    if (spec.backend == Renderers::DIRECT3D)
        return Internal::AttachD3D11(app, spec);
    if (spec.backend != Renderers::OPENGL) {
        SetInternalApiError("AttachRendererContext: use Renderers::OPENGL (WGL) or Renderers::DIRECT3D; EGL is not supported on Windows in this build.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }

    HWND const hwnd = GetHwnd(app);
    if (!hwnd) {
        SetInternalApiError("AttachRendererContext: no Win32 window (window_handle).");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    HDC const hdc = GetDC(hwnd);
    if (!hdc) {
        SetInternalApiError("AttachRendererContext: GetDC failed.");
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    const int colorBits = EffectiveColorBits(app, spec);
    const int depthBits = EffectiveDepth(app, spec);
    const int stencilBits = EffectiveStencil(app, spec);
    const int msaa = EffectiveMsaa(app, spec);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    if (spec.double_buffer)
        pfd.dwFlags |= PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = static_cast<BYTE>(colorBits >= 32 ? 32 : 24);
    pfd.cAlphaBits = static_cast<BYTE>(colorBits >= 32 ? 8 : 0);
    pfd.cDepthBits = static_cast<BYTE>(depthBits);
    pfd.cStencilBits = static_cast<BYTE>(stencilBits);

    const int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf == 0) {
        ReleaseDC(hwnd, hdc);
        SetInternalApiError("AttachRendererContext: ChoosePixelFormat failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }
    if (SetPixelFormat(hdc, pf, &pfd) == FALSE) {
        ReleaseDC(hwnd, hdc);
        SetInternalApiError("AttachRendererContext: SetPixelFormat failed.");
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    HGLRC tempRc = wglCreateContext(hdc);
    if (!tempRc) {
        ReleaseDC(hwnd, hdc);
        SetInternalApiError("AttachRendererContext: wglCreateContext failed.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }
    if (wglMakeCurrent(hdc, tempRc) == FALSE) {
        wglDeleteContext(tempRc);
        ReleaseDC(hwnd, hdc);
        SetInternalApiError("AttachRendererContext: wglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    auto const wglCreateContextAttribsARB = reinterpret_cast<PFN_wglCreateContextAttribsARB>(
        wglGetProcAddress("wglCreateContextAttribsARB"));

    HGLRC glrc = nullptr;
    if (wglCreateContextAttribsARB) {
        const int maj = spec.gl_major > 0 ? spec.gl_major : 3;
        const int min = spec.gl_minor >= 0 ? spec.gl_minor : 3;
        int flags = 0;
        if (spec.gl_forward_compatible && spec.gl_core_profile)
            flags |= static_cast<int>(WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);
        const int profileMask = spec.gl_core_profile ? static_cast<int>(WGL_CONTEXT_CORE_PROFILE_BIT_ARB)
                                                     : static_cast<int>(WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB);
        const int ctxAttr[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,
            maj,
            WGL_CONTEXT_MINOR_VERSION_ARB,
            min,
            WGL_CONTEXT_PROFILE_MASK_ARB,
            profileMask,
            WGL_CONTEXT_FLAGS_ARB,
            flags,
            0,
        };
        HGLRC const newRc = wglCreateContextAttribsARB(hdc, nullptr, ctxAttr);
        if (newRc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempRc);
            if (wglMakeCurrent(hdc, newRc) == FALSE) {
                wglDeleteContext(newRc);
                ReleaseDC(hwnd, hdc);
                SetInternalApiError("AttachRendererContext: wglMakeCurrent (attrib context) failed.");
                return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
            }
            glrc = newRc;
        } else {
            glrc = tempRc;
        }
    } else {
        glrc = tempRc;
    }

    if (!glrc) {
        ReleaseDC(hwnd, hdc);
        SetInternalApiError("AttachRendererContext: no usable GL context.");
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    auto const wglSwapIntervalEXT = reinterpret_cast<PFN_wglSwapIntervalEXT>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(spec.vsync ? 1 : 0);

    app.device = reinterpret_cast<HonkordGL_GW_Handle>(glrc);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(hdc);
    app.color_bits = colorBits;
    app.depth_bits = depthBits;
    app.stencil_bits = stencilBits;
    app.msaa_samples = msaa > 0 ? msaa : 1;
    app.vsync_enabled = spec.vsync;
    app.active_renderer = static_cast<int>(Renderers::OPENGL);

    return static_cast<int>(RendererContextResult::OK);
}
void DetachRendererContext(ApplicationContextSettings& app) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32) {
        ClearRendererHandles(app);
        return;
    }

    if (app.active_renderer == static_cast<int>(Renderers::DIRECT3D)) {
        Internal::DetachD3D11(app);
        return;
    }

    HWND const hwnd = GetHwnd(app);
    HDC const hdc = GetHdc(app);
    HGLRC const hglrc = GetHglrc(app);
    if (!hdc || !hglrc) {
        ClearRendererHandles(app);
        return;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    if (hwnd)
        ReleaseDC(hwnd, hdc);
    ClearRendererHandles(app);
}
int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Win32) {
        SetInternalApiError("RendererContextMakeCurrent: native platform is not Win32.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
    }

    if (app.active_renderer == static_cast<int>(Renderers::DIRECT3D))
        return Internal::RendererContextMakeCurrentD3D11(app);

    HWND const hwnd = GetHwnd(app);
    HDC const hdc = GetHdc(app);
    HGLRC const hglrc = GetHglrc(app);
    if (!hwnd || !hdc || !hglrc) {
        SetInternalApiError("RendererContextMakeCurrent: no WGL context.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }

    if (wglMakeCurrent(hdc, hglrc) == FALSE) {
        SetInternalApiError("RendererContextMakeCurrent: wglMakeCurrent failed.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::DIRECT3D)) {
        Internal::RendererContextSwapBuffersD3D11(app);
        return;
    }
    HDC const hdc = GetHdc(app);
    if (!hdc)
        return;
    SwapBuffers(hdc);
}

} // namespace HonkordGL::Graphics

#endif // defined(_WIN32)