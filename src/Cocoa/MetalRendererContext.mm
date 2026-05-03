/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Metal implementation for RendererContext (macOS)
 * Copyright (C) 2026 Honkord
 *
 * Link: -framework Metal -framework QuartzCore -framework Cocoa
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/WindowApplication.h>

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace HonkordGL::Graphics::Internal {

namespace {

@interface HonkordMetalRendererState : NSObject
@property (nonatomic, strong) id<MTLDevice> mtlDevice;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) CAMetalLayer *metalLayer;
@end

@implementation HonkordMetalRendererState
@end

inline NSWindow * GetNSWindow(const ApplicationContextSettings& app) noexcept
{
    return reinterpret_cast<NSWindow *>(app.window_handle);
}
void UpdateDrawableSize(NSView * view, CAMetalLayer * layer) noexcept
{
    if (!view || !layer)
        return;
    const CGFloat scale = view.window ? view.window.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor;
    const NSRect rect = [view bounds];
    layer.drawableSize = CGSizeMake(CGRectGetWidth(rect) * scale, CGRectGetHeight(rect) * scale);
}

} // namespace
int AttachRendererContextMetal(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    if (static_cast<NativePlatform>(app.native_platform) != NativePlatform::Cocoa) {
        SetInternalApiError("AttachRendererContext: Metal is only supported with Cocoa windows.");
        return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
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

    @autoreleasepool {
        id<MTLDevice> const dev = MTLCreateSystemDefaultDevice();
        if (!dev) {
            SetInternalApiError("AttachRendererContext: MTLCreateSystemDefaultDevice failed.");
            return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
        }

        id<MTLCommandQueue> const queue = [dev newCommandQueue];
        if (!queue) {
            SetInternalApiError("AttachRendererContext: newCommandQueue failed.");
            return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
        }

        CAMetalLayer * const layer = [CAMetalLayer layer];
        layer.device = dev;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
        if (@available(macOS 10.13, *))
            layer.maximumDrawableCount = spec.double_buffer ? 3 : 2;

        [view setWantsLayer:YES];
        [view setLayer:layer];

        HonkordMetalRendererState * const holder = [[HonkordMetalRendererState alloc] init];
        holder.mtlDevice = dev;
        holder.commandQueue = queue;
        holder.metalLayer = layer;

        UpdateDrawableSize(view, layer);

        if (@available(macOS 12.0, *))
            layer.displaySyncEnabled = spec.vsync ? YES : NO;

        app.renderer_private = reinterpret_cast<HonkordGL_GW_Handle>((__bridge_retained void *)holder);
        app.device = reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)dev);
        app.surface = reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)layer);
        app.active_renderer = static_cast<int>(Renderers::METAL);
        app.color_bits = 32;
        app.depth_bits = 0;
        app.stencil_bits = 0;
        app.msaa_samples = 1;
        app.vsync_enabled = spec.vsync;
        app.frame_buffer_width = static_cast<int>(layer.drawableSize.width);
        app.frame_buffer_height = static_cast<int>(layer.drawableSize.height);

        return static_cast<int>(RendererContextResult::OK);
    }
}
void DetachRendererContextMetal(ApplicationContextSettings& app) noexcept
{
    if (!app.renderer_private) {
        app.device = nullptr;
        app.surface = nullptr;
        app.active_renderer = 0;
        return;
    }

    HonkordMetalRendererState * const st = (__bridge_transfer HonkordMetalRendererState *)app.renderer_private;
    app.renderer_private = nullptr;

    NSWindow * const win = GetNSWindow(app);
    NSView * const view = win ? [win contentView] : nil;

    if (view && st.metalLayer && view.layer == st.metalLayer) {
        [view setLayer:nil];
    }

    app.device = nullptr;
    app.surface = nullptr;
    app.active_renderer = 0;
}
int RendererContextMakeCurrentMetal(ApplicationContextSettings& app) noexcept
{
    (void)app;
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffersMetal(ApplicationContextSettings& app) noexcept
{
    if (!app.renderer_private)
        return;
    HonkordMetalRendererState * const st = (__bridge HonkordMetalRendererState *)app.renderer_private;
    if (!st.mtlDevice || !st.commandQueue || !st.metalLayer)
        return;

    NSWindow * const win = GetNSWindow(app);
    NSView * const view = win ? [win contentView] : nil;
    if (!view || view.layer != st.metalLayer)
        return;

    UpdateDrawableSize(view, st.metalLayer);

    id<CAMetalDrawable> const drawable = [st.metalLayer nextDrawable];
    if (!drawable)
        return;

    MTLRenderPassDescriptor * const pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.15, 0.18, 0.28, 1.0);
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLCommandBuffer> const cmd = [st.commandQueue commandBuffer];
    if (!cmd)
        return;

    id<MTLRenderCommandEncoder> const enc = [cmd renderCommandEncoderWithDescriptor:pass];
    if (enc)
        [enc endEncoding];

    [cmd presentDrawable:drawable];
    [cmd commit];

    app.frame_buffer_width = static_cast<int>(st.metalLayer.drawableSize.width);
    app.frame_buffer_height = static_cast<int>(st.metalLayer.drawableSize.height);
}

} // namespace HonkordGL::Graphics::Internal

#endif // __APPLE__