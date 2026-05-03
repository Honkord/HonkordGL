/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — NSOpenGL implementation for RendererContext (macOS)
    * Copyright (C) 2026 Honkord
    *
    * Link: -framework Cocoa -framework OpenGL; for `Renderers::METAL` also compile and link
    * `MetalRendererContext.mm` with -framework Metal -framework QuartzCore
    */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

#include <vector>

namespace HonkordGL::Graphics {

namespace Internal {
int AttachRendererContextMetal(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept;
void DetachRendererContextMetal(ApplicationContextSettings& app) noexcept;
int RendererContextMakeCurrentMetal(ApplicationContextSettings& app) noexcept;
void RendererContextSwapBuffersMetal(ApplicationContextSettings& app) noexcept;
}

namespace {

inline NSOpenGLContext * GetGLContext(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<NSOpenGLContext *>(app.device);
}
inline NSWindow * GetNSWindow(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<NSWindow *>(app.window_handle);
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
int OpenGLProfileAttribute(const RendererContextSettings& spec) noexcept
{
    const int maj = spec.gl_major > 0 ? spec.gl_major : 3;
    const int min = spec.gl_minor >= 0 ? spec.gl_minor : 3;
    if (maj > 3 || (maj == 3 && min > 2))
        return NSOpenGLProfileVersion4_1Core;
    return NSOpenGLProfileVersion3_2Core;
}

} // namespace

int AttachRendererContext(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    {
        const int vr = ValidateRendererDeviceRequest(spec);
        if (vr != static_cast<int>(RendererContextResult::OK))
            return vr;
    }
    if (spec.backend == Renderers::METAL)
        return Internal::AttachRendererContextMetal(app, spec);

    if (spec.backend != Renderers::OPENGL) {
        SetInternalApiError("AttachRendererContext: use Renderers::OPENGL (NSOpenGL) or METAL; EGL is not supported on macOS.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
    }

    if (app.device || app.renderer_private) {
        SetInternalApiError("AttachRendererContext: renderer already attached.");
        return static_cast<int>(RendererContextResult::ALREADY_ATTACHED);
    }

    NSWindow * const win = GetNSWindow(app);
    if (!win) {
        SetInternalApiError("AttachRendererContext: no NSWindow handle.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    NSView * const view = [win contentView];
    if (!view) {
        SetInternalApiError("AttachRendererContext: window has no content view.");
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    const int colorBits = EffectiveColorBits(app, spec);
    const int depthBits = EffectiveDepth(app, spec);
    const int stencilBits = EffectiveStencil(app, spec);
    const int msaa = EffectiveMsaa(app, spec);
    const int profileAttr = OpenGLProfileAttribute(spec);

    @autoreleasepool {
        std::vector<NSOpenGLPixelFormatAttribute> attrs;
        attrs.reserve(32);
        attrs.push_back(NSOpenGLPFAOpenGLProfile);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(profileAttr));
        attrs.push_back(NSOpenGLPFAColorSize);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(colorBits >= 32 ? 32 : 24));
        attrs.push_back(NSOpenGLPFAAlphaSize);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(colorBits >= 32 ? 8 : 0));
        attrs.push_back(NSOpenGLPFADepthSize);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(depthBits));
        attrs.push_back(NSOpenGLPFAStencilSize);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(stencilBits));
        attrs.push_back(NSOpenGLPFADoubleBuffer);
        attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(spec.double_buffer ? 1 : 0));
        attrs.push_back(NSOpenGLPFAAccelerated);
        attrs.push_back(1);
        if (msaa > 1) {
            attrs.push_back(NSOpenGLPFASampleBuffers);
            attrs.push_back(1);
            attrs.push_back(NSOpenGLPFASamples);
            attrs.push_back(static_cast<NSOpenGLPixelFormatAttribute>(msaa));
        }
        attrs.push_back(0);

        NSOpenGLPixelFormat * const pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs.data()];
        if (!pf) {
            SetInternalApiError("AttachRendererContext: NSOpenGLPixelFormat failed.");
            return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
        }

        NSOpenGLContext * const ctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
        if (!ctx) {
            SetInternalApiError("AttachRendererContext: NSOpenGLContext creation failed.");
            return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
        }

        [view setWantsBestResolutionOpenGLSurface:YES];
        [ctx setView:view];
        [ctx makeCurrentContext];

        GLint swapInterval = spec.vsync ? 1 : 0;
        [ctx setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];

        app.device = reinterpret_cast<HonkordGL_GW_Handle>((__bridge_retained void *)ctx);
        app.renderer_private = nullptr;
        app.active_renderer = static_cast<int>(Renderers::OPENGL);
        app.color_bits = colorBits >= 32 ? 32 : 24;
        app.depth_bits = depthBits;
        app.stencil_bits = stencilBits;
        app.msaa_samples = msaa > 0 ? msaa : 1;
        app.vsync_enabled = spec.vsync;

        return static_cast<int>(RendererContextResult::OK);
    }
}
void DetachRendererContext(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::METAL)) {
        Internal::DetachRendererContextMetal(app);
        return;
    }

    NSOpenGLContext * const ctx = GetGLContext(app);
    if (!ctx) {
        ClearRendererHandles(app);
        return;
    }
    [NSOpenGLContext clearCurrentContext];
    [ctx clearDrawable];
    NSOpenGLContext * const transferred = (__bridge_transfer NSOpenGLContext *)app.device;
    (void)transferred;
    ClearRendererHandles(app);
}
int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::METAL))
        return Internal::RendererContextMakeCurrentMetal(app);

    NSOpenGLContext * const ctx = GetGLContext(app);
    NSWindow * const win = GetNSWindow(app);
    if (!ctx || !win) {
        SetInternalApiError("RendererContextMakeCurrent: no NSOpenGLContext or window.");
        return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
    }

    NSView * const view = [win contentView];
    if (!view) {
        SetInternalApiError("RendererContextMakeCurrent: no content view.");
        return static_cast<int>(RendererContextResult::MAKE_CURRENT_FAILED);
    }

    [ctx setView:view];
    [ctx makeCurrentContext];
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept
{
    if (app.active_renderer == static_cast<int>(Renderers::METAL)) {
        Internal::RendererContextSwapBuffersMetal(app);
        return;
    }

    NSOpenGLContext * const ctx = GetGLContext(app);
    if (!ctx)
        return;
    [ctx flushBuffer];
}

} // namespace HonkordGL::Graphics

#endif