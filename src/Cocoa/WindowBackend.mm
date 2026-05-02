/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa / AppKit window backend
    * Copyright (C) 2026 Honkord
    */

#include "WindowBackend.hpp"
#include "MonitorsCocoa.hpp"
#include "PlatformSession.hpp"
#include "DisplayMonitorPoll.hpp"

#include <HonkordGL/Events.h>
#include <HonkordGL/Video.h>

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace HonkordGL::Graphics::Cocoa {

namespace {

constexpr int kFallbackDpi = 72;

static void ApplyCocoaWindowIcon(NSWindow * win, const char * path) noexcept
{
    if (!path || !path[0])
        return;
    @autoreleasepool {
        NSString * const nsPath = [NSString stringWithUTF8String:path];
        NSImage * const img = [[NSImage alloc] initWithContentsOfFile:nsPath];
        if (!img)
            return;
        [NSApp setApplicationIconImage:img];
        if (win) {
            NSButton * const docBtn = [win standardWindowButton:NSWindowDocumentIconButton];
            if (docBtn)
                [docBtn setImage:img];
        }
    }
}

} // namespace

WindowBackend::~WindowBackend()
{
    CloseDisplay();
}
WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : display_open_(other.display_open_),
      owns_display_(other.owns_display_),
      screen_(other.screen_),
      monitor_poll_enabled_(other.monitor_poll_enabled_),
      ext_xkb_(other.ext_xkb_),
      ext_xrandr_(other.ext_xrandr_),
      ext_xinput2_(other.ext_xinput2_),
      xi_opcode_(other.xi_opcode_),
      xi2_major_(other.xi2_major_),
      xi2_minor_(other.xi2_minor_),
      xrandr_event_base_(other.xrandr_event_base_),
      xrandr_error_base_(other.xrandr_error_base_),
      cocoa_cursors_(std::move(other.cocoa_cursors_))
{
    other.display_open_ = false;
    other.owns_display_ = false;
    other.screen_ = 0;
    other.monitor_poll_enabled_ = false;
    other.ext_xkb_ = false;
    other.ext_xrandr_ = false;
    other.ext_xinput2_ = false;
    other.xi_opcode_ = 0;
    other.xi2_major_ = other.xi2_minor_ = 0;
    other.xrandr_event_base_ = other.xrandr_error_base_ = 0;
}
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        display_open_ = other.display_open_;
        owns_display_ = other.owns_display_;
        screen_ = other.screen_;
        monitor_poll_enabled_ = other.monitor_poll_enabled_;
        ext_xkb_ = other.ext_xkb_;
        ext_xrandr_ = other.ext_xrandr_;
        ext_xinput2_ = other.ext_xinput2_;
        xi_opcode_ = other.xi_opcode_;
        xi2_major_ = other.xi2_major_;
        xi2_minor_ = other.xi2_minor_;
        xrandr_event_base_ = other.xrandr_event_base_;
        xrandr_error_base_ = other.xrandr_error_base_;
        cocoa_cursors_ = std::move(other.cocoa_cursors_);
        other.display_open_ = false;
        other.owns_display_ = false;
        other.screen_ = 0;
        other.monitor_poll_enabled_ = false;
        other.ext_xkb_ = false;
        other.ext_xrandr_ = false;
        other.ext_xinput2_ = false;
        other.xi_opcode_ = 0;
        other.xi2_major_ = other.xi2_minor_ = 0;
        other.xrandr_event_base_ = other.xrandr_error_base_ = 0;
    }
    return *this;
}
void WindowBackend::initializeCocoaExtensions() noexcept
{
    ext_xkb_ = true;
    ext_xrandr_ = true;
    ext_xinput2_ = false;
    xi_opcode_ = 0;
    xi2_major_ = 0;
    xi2_minor_ = 0;
    xrandr_event_base_ = 0;
    xrandr_error_base_ = 0;
}
void * WindowBackend::NativeWindow(const ApplicationContextSettings& ctx) noexcept
{
    return ctx.window_handle;
}
bool WindowBackend::OpenDisplay(const char * app_name) noexcept
{
    (void)app_name;
    if (display_open_)
        return true;
    @autoreleasepool {
        [NSApplication sharedApplication];
        if ([NSApp setActivationPolicy:NSApplicationActivationPolicyRegular] != YES) {
            HonkordGL::Graphics::SetInternalApiError("NSApplication setActivationPolicy failed.");
            return false;
        }
        display_open_ = true;
        owns_display_ = true;
        screen_ = 0;
        initializeCocoaExtensions();
        return true;
    }
}
bool WindowBackend::BorrowFromSession(Internal::PlatformSession& session) noexcept
{
    if (!session.is_connected()) {
        HonkordGL::Graphics::SetInternalApiError("BorrowFromSession: Cocoa PlatformSession is not connected.");
        return false;
    }
    CloseDisplay();
    display_open_ = true;
    owns_display_ = false;
    screen_ = session.default_screen();
    initializeCocoaExtensions();
    return true;
}
void WindowBackend::CloseDisplay() noexcept
{
    clearCocoaCursors();
    if (monitor_poll_enabled_) {
        Internal::CocoaDisableMonitorPolling();
        monitor_poll_enabled_ = false;
    }
    display_open_ = false;
    owns_display_ = false;
    screen_ = 0;
    ext_xkb_ = false;
    ext_xrandr_ = false;
    ext_xinput2_ = false;
    xi_opcode_ = 0;
    xi2_major_ = xi2_minor_ = 0;
    xrandr_event_base_ = xrandr_error_base_ = 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    if (!display_open_ || !ext_xrandr_)
        return false;
    if (!Internal::CocoaEnableMonitorPolling())
        return false;
    monitor_poll_enabled_ = true;
    return true;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (!display_open_ || !ext_xrandr_)
        return false;
    return Internal::CocoaPollMonitorTopologyChanged();
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (!display_open_)
        return 0;
    return Internal::GetMonitorCount();
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!display_open_ || !out)
        return false;
    return Internal::GetMonitorBounds(index, out);
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    const char * const title = ctx.window_title;
    const int x = ctx.client_rect.x;
    const int y = ctx.client_rect.y;
    const unsigned width = static_cast<unsigned>(ctx.client_rect.w);
    const unsigned height = static_cast<unsigned>(ctx.client_rect.z);

    if (!display_open_ || width == 0 || height == 0) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: display not open or zero size.");
        return false;
    }

    @autoreleasepool {
        NSArray * const screens = [NSScreen screens];
        if (!screens || [screens count] == 0) {
            HonkordGL::Graphics::SetInternalApiError("CreateWindow: no NSScreen available.");
            return false;
        }
        NSScreen * const screen = [screens objectAtIndex:static_cast<NSUInteger>(screen_)];
        if (!screen) {
            HonkordGL::Graphics::SetInternalApiError("CreateWindow: invalid screen index.");
            return false;
        }
        const NSRect sf = [screen frame];
        const CGFloat ybl = NSMaxY(sf) - static_cast<CGFloat>(y) - static_cast<CGFloat>(height);
        const NSRect frame = NSMakeRect(
            static_cast<CGFloat>(x) + NSMinX(sf),
            ybl,
            static_cast<CGFloat>(width),
            static_cast<CGFloat>(height));

        const NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        NSWindow * const win = [[NSWindow alloc] initWithContentRect:frame
                                                            styleMask:style
                                                              backing:NSBackingStoreBuffered
                                                                defer:NO];
        if (!win) {
            HonkordGL::Graphics::SetInternalApiError("CreateWindow: NSWindow allocation failed.");
            return false;
        }

        if (title && std::strlen(title) > 0)
            [win setTitle:[NSString stringWithUTF8String:title]];
        else
            [win setTitle:@""];

        if (ctx.window_icon_path && std::strlen(ctx.window_icon_path) > 0)
            ApplyCocoaWindowIcon(win, ctx.window_icon_path);

        [win center];
        [win makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>((__bridge_retained void *)win);
        ctx.client_rect.x = x;
        ctx.client_rect.y = y;
        ctx.client_rect.w = static_cast<int>(width);
        ctx.client_rect.z = static_cast<int>(height);
        ctx.is_visible = 1;
        ctx.is_minimized = 0;

        NSView * const cv = [win contentView];
        if (cv) {
            const NSSize bs = [win convertSizeToBacking:cv.bounds.size];
            ctx.frame_buffer_width = static_cast<int>(std::lround(bs.width));
            ctx.frame_buffer_height = static_cast<int>(std::lround(bs.height));
        } else {
            ctx.frame_buffer_width = static_cast<int>(width);
            ctx.frame_buffer_height = static_cast<int>(height);
        }
        const CGFloat scale = [win backingScaleFactor];
        ctx.dpi_x = static_cast<int>(72.0 * scale);
        ctx.dpi_y = static_cast<int>(72.0 * scale);
        if (ctx.dpi_x <= 0)
            ctx.dpi_x = kFallbackDpi;
        if (ctx.dpi_y <= 0)
            ctx.dpi_y = kFallbackDpi;
        ctx.needs_resize = 0;
        ctx.native_platform = static_cast<int>(NativePlatform::Cocoa);
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)[NSApplication sharedApplication]);
        return true;
    }
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!ctx.window_handle)
        return;
    releaseCocoaCursor(ctx.window_handle);
    @autoreleasepool {
        NSWindow * const win = (__bridge_transfer NSWindow *)ctx.window_handle;
        ctx.window_handle = nullptr;
        if (ctx.device_context == reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)[NSApplication sharedApplication]))
            ctx.device_context = nullptr;
        [win close];
    }
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    if (!display_open_ || !ctx.window_handle)
        return false;

    if (!ctx.device_context)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)[NSApplication sharedApplication]);

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == HonkordGL::Events::EventType::QUIT)
            return false;
    }
    return true;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    if (!display_open_ || !ctx.window_handle || !title)
        return;
    @autoreleasepool {
        NSWindow * const win = reinterpret_cast<NSWindow *>(ctx.window_handle);
        [win setTitle:[NSString stringWithUTF8String:title]];
    }
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    if (!display_open_ || !ctx.window_handle)
        return;
    @autoreleasepool {
        NSWindow * const win = reinterpret_cast<NSWindow *>(ctx.window_handle);
        if (ctx.window_title && std::strlen(ctx.window_title) > 0)
            [win setTitle:[NSString stringWithUTF8String:ctx.window_title]];
        if (ctx.window_icon_path && std::strlen(ctx.window_icon_path) > 0)
            ApplyCocoaWindowIcon(win, ctx.window_icon_path);
        const int w = ctx.client_rect.w;
        const int h = ctx.client_rect.z;
        if (w > 0 && h > 0) {
            NSArray * const screens = [NSScreen screens];
            NSScreen * screen = [win screen];
            if (!screen && screens && [screens count] > 0)
                screen = [screens objectAtIndex:static_cast<NSUInteger>(screen_)];
            if (!screen)
                screen = [NSScreen mainScreen];
            if (screen) {
                const NSRect sf = [screen frame];
                const CGFloat ybl = NSMaxY(sf) - static_cast<CGFloat>(ctx.client_rect.y) - static_cast<CGFloat>(h);
                const NSRect frame = NSMakeRect(
                    static_cast<CGFloat>(ctx.client_rect.x) + NSMinX(sf),
                    ybl,
                    static_cast<CGFloat>(w),
                    static_cast<CGFloat>(h));
                [win setFrame:frame display:YES];
            }
        }
        NSView * const cv = [win contentView];
        if (cv) {
            const NSSize bs = [win convertSizeToBacking:cv.bounds.size];
            ctx.frame_buffer_width = static_cast<int>(std::lround(bs.width));
            ctx.frame_buffer_height = static_cast<int>(std::lround(bs.height));
        } else if (w > 0 && h > 0) {
            ctx.frame_buffer_width = w;
            ctx.frame_buffer_height = h;
        }
    }
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)[NSApplication sharedApplication]);
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!display_open_ || !out)
        return false;
    if (!CreateIpcHelperWindow(
            reinterpret_cast<HonkordGL_GW_Handle>((__bridge void *)[NSApplication sharedApplication]),
            screen_,
            message_atom_name,
            out)) {
        HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow failed.");
        return false;
    }
    return true;
}
void * WindowBackend::GetDisplay() const noexcept
{
    if (!display_open_)
        return nullptr;
    return (__bridge void *)[NSApplication sharedApplication];
}

namespace {

using HonkordGL::Graphics::CursorKind;
using HonkordGL::Graphics::CursorImageParams;

void ResizeNearestRgba(
    const std::uint8_t * src,
    int sw,
    int sh,
    int sstride,
    int dw,
    int dh,
    std::vector<std::uint8_t> * out) noexcept
{
    out->resize(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh) * 4u);
    for (int y = 0; y < dh; ++y) {
        const int sy = (y * sh) / dh;
        for (int x = 0; x < dw; ++x) {
            const int sx = (x * sw) / dw;
            const std::uint8_t * const p = src + static_cast<std::size_t>(sy) * static_cast<std::size_t>(sstride) + static_cast<std::size_t>(sx) * 4u;
            std::uint8_t * const d = out->data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(dw) + static_cast<std::size_t>(x)) * 4u;
            std::memcpy(d, p, 4u);
        }
    }
}

/** Associates `cursor` with the window’s content view so each NSWindow keeps its own cursor (multi-window). */
static void ApplyCursorToCocoaWindow(NSWindow * win, NSCursor * cursor) noexcept
{
    if (!win || !cursor)
        return;
    NSView * const cv = [win contentView];
    if (cv) {
        [cv discardCursorRects];
        [cv addCursorRect:[cv bounds] cursor:cursor];
    }
    [cursor set];
}

NSCursor * NSCursorForKind(CursorKind k) noexcept
{
    switch (k) {
    case CursorKind::Arrow:
    case CursorKind::ArrowWait:
    case CursorKind::Wait:
        return [NSCursor arrowCursor];
    case CursorKind::Text:
        return [NSCursor IBeamCursor];
    case CursorKind::Hand:
        return [NSCursor pointingHandCursor];
    case CursorKind::SizeHorizontal:
    case CursorKind::SizeLeft:
    case CursorKind::SizeRight:
        return [NSCursor resizeLeftRightCursor];
    case CursorKind::SizeVertical:
    case CursorKind::SizeTop:
    case CursorKind::SizeBottom:
        return [NSCursor resizeUpDownCursor];
    case CursorKind::SizeTopLeftBottomRight:
    case CursorKind::SizeTopLeft:
    case CursorKind::SizeBottomRight:
        return [NSCursor resizeUpLeftDownRightCursor];
    case CursorKind::SizeBottomLeftTopRight:
    case CursorKind::SizeBottomLeft:
    case CursorKind::SizeTopRight:
        return [NSCursor resizeUpRightDownLeftCursor];
    case CursorKind::SizeAll:
        return [NSCursor closedHandCursor];
    case CursorKind::Cross:
        return [NSCursor crosshairCursor];
    case CursorKind::Help:
        return [NSCursor arrowCursor];
    case CursorKind::NotAllowed:
        return [NSCursor operationNotAllowedCursor];
    default:
        return [NSCursor arrowCursor];
    }
}

} // namespace

void WindowBackend::releaseCocoaCursor(void * winKey) noexcept
{
    if (!winKey)
        return;
    const auto it = cocoa_cursors_.find(winKey);
    if (it == cocoa_cursors_.end())
        return;
    if (it->second) {
        (void)(__bridge_transfer NSCursor *)it->second;
    }
    cocoa_cursors_.erase(it);
}

void WindowBackend::clearCocoaCursors() noexcept
{
    for (auto& e : cocoa_cursors_) {
        if (e.second)
            (void)(__bridge_transfer NSCursor *)e.second;
    }
    cocoa_cursors_.clear();
}

void WindowBackend::SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept
{
    if (!display_open_ || !ctx.window_handle)
        return;
    releaseCocoaCursor(ctx.window_handle);
    @autoreleasepool {
        NSCursor * const c = NSCursorForKind(kind);
        ApplyCursorToCocoaWindow(reinterpret_cast<NSWindow *>(ctx.window_handle), c);
    }
}

bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept
{
    if (!display_open_ || !ctx.window_handle || !params.rgba_pixels)
        return false;
    int w = params.width;
    int h = params.height;
    if (w <= 0 || h <= 0)
        return false;
    const int sstride = params.stride_bytes > 0 ? params.stride_bytes : (w * 4);
    if (sstride < w * 4)
        return false;

    int dw = params.display_width > 0 ? params.display_width : w;
    int dh = params.display_height > 0 ? params.display_height : h;
    if (dw <= 0)
        dw = w;
    if (dh <= 0)
        dh = h;

    const std::uint8_t * src = params.rgba_pixels;
    std::vector<std::uint8_t> resized;
    if (dw != w || dh != h) {
        ResizeNearestRgba(params.rgba_pixels, w, h, sstride, dw, dh, &resized);
        src = resized.data();
        w = dw;
        h = dh;
    }

    int hx = params.hotspot_x;
    int hy = params.hotspot_y;
    if (dw != params.width || dh != params.height) {
        if (params.width > 0 && params.height > 0) {
            hx = (hx * dw) / params.width;
            hy = (hy * dh) / params.height;
        }
    }
    hx = (std::max)(0, (std::min)(hx, w - 1));
    hy = (std::max)(0, (std::min)(hy, h - 1));

    @autoreleasepool {
        unsigned char * planes[5] = { nullptr };
        NSBitmapImageRep * const rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:planes
            pixelsWide:w
            pixelsHigh:h
            bitsPerSample:8
            samplesPerPixel:4
            hasAlpha:YES
            isPlanar:NO
            colorSpaceName:NSCalibratedRGBColorSpace
            bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
            bytesPerRow:w * 4
            bitsPerPixel:32];
        if (!rep)
            return false;
        unsigned char * bits = [rep bitmapData];
        if (!bits)
            return false;
        const int rowBytes = static_cast<int>([rep bytesPerRow]);
        for (int y = 0; y < h; ++y) {
            const std::uint8_t * row = src + static_cast<std::size_t>(y) * static_cast<std::size_t>(w * 4);
            unsigned char * dst = bits + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowBytes);
            std::memcpy(dst, row, static_cast<std::size_t>(w) * 4u);
        }

        const CGFloat dispW = static_cast<CGFloat>(params.display_width > 0 ? params.display_width : w);
        const CGFloat dispH = static_cast<CGFloat>(params.display_height > 0 ? params.display_height : h);
        NSImage * const img = [[NSImage alloc] initWithSize:NSMakeSize(dispW, dispH)];
        [img addRepresentation:rep];
        NSCursor * const cur = [[NSCursor alloc] initWithImage:img hotSpot:NSMakePoint(static_cast<CGFloat>(hx), static_cast<CGFloat>(hy))];
        if (!cur)
            return false;
        releaseCocoaCursor(ctx.window_handle);
        cocoa_cursors_[ctx.window_handle] = (__bridge_retained void *)cur;
        ApplyCursorToCocoaWindow(reinterpret_cast<NSWindow *>(ctx.window_handle), cur);
    }
    return true;
}

void WindowBackend::ResetCursor(ApplicationContextSettings& ctx) noexcept
{
    if (!display_open_ || !ctx.window_handle)
        return;
    releaseCocoaCursor(ctx.window_handle);
    @autoreleasepool {
        ApplyCursorToCocoaWindow(reinterpret_cast<NSWindow *>(ctx.window_handle), [NSCursor arrowCursor]);
    }
}

} // namespace HonkordGL::Graphics::Cocoa

#else

namespace HonkordGL::Graphics::Cocoa {

WindowBackend::~WindowBackend() = default;
WindowBackend::WindowBackend(WindowBackend&& other) noexcept = default;
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept = default;

void WindowBackend::initializeCocoaExtensions() noexcept {}
void * WindowBackend::NativeWindow(const ApplicationContextSettings&) noexcept
{
    return nullptr;
}
bool WindowBackend::OpenDisplay(const char *) noexcept
{
    return false;
}
bool WindowBackend::BorrowFromSession(Internal::PlatformSession&) noexcept
{
    return false;
}
void WindowBackend::CloseDisplay() noexcept {}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    return false;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    return false;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    return 0;
}
bool WindowBackend::GetMonitorBounds(int, Recti *) const noexcept
{
    return false;
}
bool WindowBackend::CreateWindow(ApplicationContextSettings&) noexcept
{
    return false;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings&) noexcept {}
bool WindowBackend::ProcessEvents(ApplicationContextSettings&) noexcept
{
    return false;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings&, const char *) noexcept {}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings&) noexcept {}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings&) const noexcept {}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow *, const char *) const noexcept
{
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    return nullptr;
}
void WindowBackend::SetCursorKind(ApplicationContextSettings&, CursorKind) noexcept {}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings&, const CursorImageParams&) noexcept
{
    return false;
}
void WindowBackend::ResetCursor(ApplicationContextSettings&) noexcept {}

} // namespace HonkordGL::Graphics::Cocoa

#endif