/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — cross-platform WindowBackend (implementation)
    * Copyright (C) 2026 Honkord
    */

#include <HonkordGL/WindowBackend.h>

#if HONKORDGL_PLATFORM_ANDROID

#include "Android/WindowBackend.hpp"
#include <HonkordGL/AndroidNativeApp.h>
#include <HonkordGL/Video.h>

#include <cstdlib>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <android_native_app_glue.h>

#include <chrono>
#include <thread>

namespace HonkordGL::Graphics {

struct WindowBackend::Impl {
    WindowBackendKind kind{WindowBackendKind::Unknown};
    std::unique_ptr<Android::WindowBackend> android;
    int target_frame_rate_fps{0};
    std::chrono::steady_clock::time_point last_frame_tick{};
    std::uint64_t frame_ticks{0};
};

int GetVideoDriverCount() noexcept
{
    return 1;
}
WindowBackend::WindowBackend() noexcept
    : impl_(std::make_unique<Impl>())
{
}
WindowBackend::~WindowBackend() = default;
WindowBackend::WindowBackend(WindowBackend&& other) noexcept = default;
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept = default;

bool WindowBackend::Initialize(const char * display_or_app_name) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (impl_->kind != WindowBackendKind::Unknown)
        return true;
    ClearInternalApiError();
    {
        auto p = std::make_unique<Android::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->android = std::move(p);
            impl_->kind = WindowBackendKind::Android;
            return true;
        }
    }
    SetInternalApiError("No display driver could be initialized.");
    return false;
}
void WindowBackend::Shutdown() noexcept
{
    if (!impl_)
        return;
    impl_->android.reset();
    impl_->kind = WindowBackendKind::Unknown;
    impl_->target_frame_rate_fps = 0;
    impl_->last_frame_tick = {};
    impl_->frame_ticks = 0;
}
WindowBackendKind WindowBackend::GetKind() const noexcept
{
    return impl_ ? impl_->kind : WindowBackendKind::Unknown;
}
bool WindowBackend::OpenDisplay(const char * display_or_app_name) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (impl_->kind == WindowBackendKind::Unknown)
        return Initialize(display_or_app_name);
    if (impl_->kind == WindowBackendKind::Android && impl_->android)
        return impl_->android->OpenDisplay(display_or_app_name);
    return false;
}
bool WindowBackend::BorrowFromAndroid(struct android_app * app) noexcept
{
    if (!app)
        return false;
    Android::BindNativeApp(app);
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->android)
        impl_->android = std::make_unique<Android::WindowBackend>();
    ClearInternalApiError();
    if (!impl_->android->AttachApp(app))
        return false;
    impl_->kind = WindowBackendKind::Android;
    return true;
}
void WindowBackend::CloseDisplay() noexcept
{
    if (!impl_)
        return;
    if (impl_->android)
        impl_->android->CloseDisplay();
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return false;
    if (impl_->kind == WindowBackendKind::Android && impl_->android) {
        const bool ok = impl_->android->CreateWindow(ctx);
        if (ok)
            (void)EnableRandrMonitorPolling();
        return ok;
    }
    return false;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return;
    if (impl_->android)
        impl_->android->DestroyWindow(ctx);
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return false;
    if (impl_->kind == WindowBackendKind::Android && impl_->android)
        return impl_->android->ProcessEvents(ctx);
    return false;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    if (!impl_)
        return;
    if (impl_->kind == WindowBackendKind::Android && impl_->android)
        impl_->android->SetWindowTitle(ctx, title);
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return;
    if (impl_->kind == WindowBackendKind::Android && impl_->android)
        impl_->android->ApplyWindowSettings(ctx);
}
void WindowBackend::ResetApplicationSettings(ApplicationContextSettings& ctx, const ApplicationContextSettings& settings) noexcept
{
    if (!impl_)
        return;
    if (!ctx.window_handle) {
        ctx = settings;
        return;
    }

    const HonkordGL_GW_Handle keep_wh = ctx.window_handle;
    const HonkordGL_GW_Handle keep_dc = ctx.device_context;
    const HonkordGL_GW_Handle keep_dev = ctx.device;
    const HonkordGL_GW_Handle keep_surf = ctx.surface;
    const HonkordGL_GW_Handle keep_egl = ctx.egl_display;
    const HonkordGL_GW_Handle keep_priv = ctx.renderer_private;
    const int keep_plat = ctx.native_platform;
    const int keep_renderer = ctx.active_renderer;

    ctx = settings;
    ctx.window_handle = keep_wh;
    ctx.device_context = keep_dc;
    ctx.device = keep_dev;
    ctx.surface = keep_surf;
    ctx.egl_display = keep_egl;
    ctx.renderer_private = keep_priv;
    ctx.native_platform = keep_plat;
    ctx.active_renderer = keep_renderer;

    ApplyWindowSettings(ctx);
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    if (!impl_)
        return;
    if (impl_->android)
        impl_->android->AttachDisplayToContext(ctx);
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!impl_)
        return false;
    if (impl_->kind == WindowBackendKind::Android && impl_->android)
        return impl_->android->CreateIpcHelper(out, message_atom_name);
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    if (!impl_)
        return nullptr;
    if (impl_->android)
        return impl_->android->GetDisplay();
    return nullptr;
}
int WindowBackend::GetDefaultScreen() const noexcept
{
    if (!impl_)
        return 0;
    if (impl_->android)
        return impl_->android->GetDefaultScreen();
    return 0;
}
bool WindowBackend::HasXkbExtension() const noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->HasXkbExtension();
}
bool WindowBackend::HasXRandrExtension() const noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->HasXRandrExtension();
}
bool WindowBackend::HasXInput2Extension() const noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->HasXInput2Extension();
}
int WindowBackend::XInput2MajorVersion() const noexcept
{
    if (!impl_ || !impl_->android)
        return 0;
    return impl_->android->XInput2MajorVersion();
}
int WindowBackend::XInput2MinorVersion() const noexcept
{
    if (!impl_ || !impl_->android)
        return 0;
    return impl_->android->XInput2MinorVersion();
}
int WindowBackend::XInputOpcode() const noexcept
{
    if (!impl_ || !impl_->android)
        return 0;
    return impl_->android->XInputOpcode();
}
int WindowBackend::RandrEventBase() const noexcept
{
    if (!impl_ || !impl_->android)
        return 0;
    return impl_->android->RandrEventBase();
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->EnableRandrMonitorPolling();
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->PollMonitorsChanged();
}
bool WindowBackend::PresentSoftwareFramebuffer(
    const ApplicationContextSettings&,
    const std::uint8_t *,
    int,
    int,
    int) noexcept
{
    return false;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (!impl_ || !impl_->android)
        return 0;
    return impl_->android->GetMonitorCount();
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!impl_ || !impl_->android)
        return false;
    return impl_->android->GetMonitorBounds(index, out);
}
void WindowBackend::SetCursorKind(ApplicationContextSettings&, CursorKind) noexcept {}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings&, const CursorImageParams& params) noexcept
{
    if (!params.rgba_pixels || params.width <= 0 || params.height <= 0)
        return false;
    int const stride = params.stride_bytes > 0 ? params.stride_bytes : params.width * 4;
    if (stride < params.width * 4)
        return false;
    if (params.hotspot_x < 0 || params.hotspot_y < 0 || params.hotspot_x >= params.width
        || params.hotspot_y >= params.height)
        return false;
    return true;
}
bool WindowBackend::SetCursorFromImageFile(ApplicationContextSettings&, const char *, const CursorImageParams&) noexcept
{
    return false;
}
void WindowBackend::ResetCursor(ApplicationContextSettings&) noexcept {}

void WindowBackend::SetTargetFrameRate(int fps) noexcept
{
    if (!impl_)
        return;
    impl_->target_frame_rate_fps = fps > 0 ? fps : 0;
    impl_->last_frame_tick = {};
}

void WindowBackend::DelayFrame() noexcept
{
    if (!impl_ || impl_->target_frame_rate_fps <= 0)
        return;
    using clock = std::chrono::steady_clock;
    auto const now = clock::now();
    auto const frame_budget = std::chrono::nanoseconds(1000000000LL / impl_->target_frame_rate_fps);
    if (impl_->last_frame_tick.time_since_epoch().count() != 0) {
        auto const elapsed = now - impl_->last_frame_tick;
        if (elapsed < frame_budget)
            std::this_thread::sleep_for(frame_budget - elapsed);
    }
    impl_->last_frame_tick = clock::now();
    ++impl_->frame_ticks;
}

std::uint64_t WindowBackend::GetTicks() const noexcept
{
    return impl_ ? impl_->frame_ticks : 0;
}

void WindowBackend::SetTicks(std::uint64_t ticks) noexcept
{
    if (!impl_)
        return;
    impl_->frame_ticks = ticks;
}

} // namespace HonkordGL::Graphics

#elif HONKORDGL_PLATFORM_WINDOW_BACKEND_DESKTOP

#if HONKORDGL_PLATFORM_LINUX_DESKTOP

#include "X11/WindowBackend.hpp"
#include "X11/PlatformSession.hpp"
#include <HonkordGL/X11IpcWindow.h>

#include "Wayland/WindowBackend.hpp"
#include "Wayland/PlatformSession.hpp"
#include <HonkordGL/WaylandIpcWindow.h>

#if defined(HONKORDGL_ENABLE_DRM)
#include "DRM/WindowBackend.hpp"
#endif

#elif HONKORDGL_PLATFORM_APPLE

#include "Cocoa/WindowBackend.hpp"
#include "Cocoa/PlatformSession.hpp"
#include <HonkordGL/CocoaIpcWindow.h>

#elif HONKORDGL_PLATFORM_WINDOWS

#include "Win32/WindowBackend.hpp"

#elif HONKORDGL_PLATFORM_NETBSD

#include "NetBSD/WindowBackend.hpp"
#include "NetBSD/PlatformSession.hpp"
#include <HonkordGL/X11IpcWindow.h>

#elif HONKORDGL_PLATFORM_FREEBSD

#include "FreeBSD/WindowBackend.hpp"
#include "FreeBSD/PlatformSession.hpp"
#include <HonkordGL/X11IpcWindow.h>

#elif HONKORDGL_PLATFORM_RASPBERRY

#include "RaspberryPi/WindowBackend.hpp"
#include "RaspberryPi/PlatformSession.hpp"
#include <HonkordGL/X11IpcWindow.h>

#endif

#include <HonkordGL/Video.h>

#include <HonkordGL/third_party/stb_image.h>

#include <cstdlib>

#include <chrono>
#include <thread>

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace HonkordGL::Graphics {

struct WindowBackend::Impl {
    WindowBackendKind kind{WindowBackendKind::Unknown};
    int target_frame_rate_fps{0};
    std::chrono::steady_clock::time_point last_frame_tick{};
    std::uint64_t frame_ticks{0};
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    std::unique_ptr<X11::WindowBackend> x11;
    std::unique_ptr<Wayland::WindowBackend> wayland;
# if defined(HONKORDGL_ENABLE_DRM)
    std::unique_ptr<DRM::WindowBackend> drm;
# endif
#elif HONKORDGL_PLATFORM_NETBSD
    std::unique_ptr<NetBSD::WindowBackend> netbsd;
#elif HONKORDGL_PLATFORM_FREEBSD
    std::unique_ptr<FreeBSD::WindowBackend> freebsd;
#elif HONKORDGL_PLATFORM_RASPBERRY
    std::unique_ptr<RaspberryPi::WindowBackend> raspberry;
#elif HONKORDGL_PLATFORM_APPLE
    std::unique_ptr<Cocoa::WindowBackend> cocoa;
#elif HONKORDGL_PLATFORM_WINDOWS
    std::unique_ptr<Win32::WindowBackend> win32;
#endif
};

namespace {

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
static_assert(sizeof(IpcHelperWindow) == sizeof(X11::IpcHelperWindow), "IpcHelperWindow layout mismatch");
static_assert(sizeof(IpcHelperWindow) == sizeof(Wayland::IpcHelperWindow), "IpcHelperWindow layout mismatch");
#elif HONKORDGL_PLATFORM_NETBSD
static_assert(sizeof(IpcHelperWindow) == sizeof(X11::IpcHelperWindow), "IpcHelperWindow layout mismatch");
#elif HONKORDGL_PLATFORM_FREEBSD
static_assert(sizeof(IpcHelperWindow) == sizeof(X11::IpcHelperWindow), "IpcHelperWindow layout mismatch");
#elif HONKORDGL_PLATFORM_RASPBERRY
static_assert(sizeof(IpcHelperWindow) == sizeof(X11::IpcHelperWindow), "IpcHelperWindow layout mismatch");
#elif HONKORDGL_PLATFORM_APPLE
static_assert(sizeof(IpcHelperWindow) == sizeof(Cocoa::IpcHelperWindow), "IpcHelperWindow layout mismatch");
#endif

} // namespace

namespace {

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)
bool PresentSoftwareFramebufferX11(
    const ApplicationContextSettings& ctx,
    const std::uint8_t * rgba_pixels,
    int width,
    int height,
    int stride_bytes) noexcept
{
    if (!NativePlatformUsesX11ClientHandles(static_cast<NativePlatform>(ctx.native_platform)))
        return false;
    if (!rgba_pixels || width <= 0 || height <= 0 || stride_bytes < width * 4)
        return false;

    auto * const dpy = reinterpret_cast<Display *>(ctx.device_context);
    const ::Window win = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(ctx.window_handle));
    if (!dpy || !win)
        return false;

    const int scr = DefaultScreen(dpy);
    Visual * const vis = DefaultVisual(dpy, scr);
    const int depth = DefaultDepth(dpy, scr);
    XImage * const xi = XCreateImage(dpy, vis, depth, ZPixmap, 0, nullptr, width, height, 32, 0);
    if (!xi)
        return false;
    xi->data = static_cast<char *>(::malloc(static_cast<std::size_t>(xi->bytes_per_line) * static_cast<std::size_t>(height)));
    if (!xi->data) {
        xi->f.destroy_image(xi);
        return false;
    }

    for (int y = 0; y < height; ++y) {
        const std::uint8_t * const row = rgba_pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes);
        for (int x = 0; x < width; ++x) {
            const std::uint8_t * const p = row + static_cast<std::size_t>(x) * 4u;
            const unsigned long pixel = (static_cast<unsigned long>(p[0]) << 16u) | (static_cast<unsigned long>(p[1]) << 8u)
                | static_cast<unsigned long>(p[2]);
            (void)xi->f.put_pixel(xi, x, y, pixel);
        }
    }

    GC const gc = XCreateGC(dpy, win, 0, nullptr);
    XPutImage(dpy, win, gc, xi, 0, 0, 0, 0, static_cast<unsigned>(width), static_cast<unsigned>(height));
    XFreeGC(dpy, gc);
    (void)xi->f.destroy_image(xi);
    XFlush(dpy);
    return true;
}
#endif

} // namespace

int GetVideoDriverCount() noexcept
{
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
# if defined(HONKORDGL_ENABLE_DRM)
    return 3;
# else
    return 2;
# endif
#elif HONKORDGL_PLATFORM_NETBSD
    return 1;
#elif HONKORDGL_PLATFORM_FREEBSD
    return 1;
#elif HONKORDGL_PLATFORM_RASPBERRY
    return 1;
#elif HONKORDGL_PLATFORM_APPLE
    return 1;
#elif HONKORDGL_PLATFORM_WINDOWS
    return 1;
#else
    return 0;
#endif
}
WindowBackend::WindowBackend() noexcept
    : impl_(std::make_unique<Impl>())
{
}
WindowBackend::~WindowBackend() = default;
WindowBackend::WindowBackend(WindowBackend&& other) noexcept = default;
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept = default;

bool WindowBackend::Initialize(const char * display_or_app_name) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (impl_->kind != WindowBackendKind::Unknown)
        return true;
    ClearInternalApiError();
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    {
        auto p = std::make_unique<X11::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->x11 = std::move(p);
            impl_->kind = WindowBackendKind::X11;
            return true;
        }
    }
    {
        auto p = std::make_unique<Wayland::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->wayland = std::move(p);
            impl_->kind = WindowBackendKind::Wayland;
            return true;
        }
    }
# if defined(HONKORDGL_ENABLE_DRM)
    {
        auto p = std::make_unique<DRM::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->drm = std::move(p);
            impl_->kind = WindowBackendKind::DRM;
            return true;
        }
    }
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    {
        auto p = std::make_unique<NetBSD::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->netbsd = std::move(p);
            impl_->kind = WindowBackendKind::NetBSD;
            return true;
        }
    }
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    {
        auto p = std::make_unique<FreeBSD::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->freebsd = std::move(p);
            impl_->kind = WindowBackendKind::FreeBSD;
            return true;
        }
    }
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    {
        auto p = std::make_unique<RaspberryPi::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->raspberry = std::move(p);
            impl_->kind = WindowBackendKind::RaspberryPi;
            return true;
        }
    }
#endif
#if HONKORDGL_PLATFORM_APPLE
    {
        auto p = std::make_unique<Cocoa::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->cocoa = std::move(p);
            impl_->kind = WindowBackendKind::Cocoa;
            return true;
        }
    }
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    {
        auto p = std::make_unique<Win32::WindowBackend>();
        if (p->OpenDisplay(display_or_app_name)) {
            impl_->win32 = std::move(p);
            impl_->kind = WindowBackendKind::Win32;
            return true;
        }
    }
#endif
    SetInternalApiError("No display driver could be initialized.");
    return false;
}
void WindowBackend::Shutdown() noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    impl_->x11.reset();
    impl_->wayland.reset();
# if defined(HONKORDGL_ENABLE_DRM)
    impl_->drm.reset();
# endif
#elif HONKORDGL_PLATFORM_NETBSD
    impl_->netbsd.reset();
#elif HONKORDGL_PLATFORM_FREEBSD
    impl_->freebsd.reset();
#elif HONKORDGL_PLATFORM_RASPBERRY
    impl_->raspberry.reset();
#elif HONKORDGL_PLATFORM_APPLE
    impl_->cocoa.reset();
#elif HONKORDGL_PLATFORM_WINDOWS
    impl_->win32.reset();
#endif
    impl_->kind = WindowBackendKind::Unknown;
    impl_->target_frame_rate_fps = 0;
    impl_->last_frame_tick = {};
    impl_->frame_ticks = 0;
}
WindowBackendKind WindowBackend::GetKind() const noexcept
{
    return impl_ ? impl_->kind : WindowBackendKind::Unknown;
}
bool WindowBackend::OpenDisplay(const char * display_or_app_name) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (impl_->kind == WindowBackendKind::Unknown)
        return Initialize(display_or_app_name);
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        return impl_->x11->OpenDisplay(display_or_app_name);
    if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        return impl_->wayland->OpenDisplay(display_or_app_name);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        return impl_->drm->OpenDisplay(display_or_app_name);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        return impl_->netbsd->OpenDisplay(display_or_app_name);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        return impl_->freebsd->OpenDisplay(display_or_app_name);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        return impl_->raspberry->OpenDisplay(display_or_app_name);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        return impl_->cocoa->OpenDisplay(display_or_app_name);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        return impl_->win32->OpenDisplay(display_or_app_name);
#endif
    return false;
}
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
bool WindowBackend::BorrowFromSession(X11::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->x11)
        impl_->x11 = std::make_unique<X11::WindowBackend>();
    if (!impl_->x11->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::X11;
    return true;
}
bool WindowBackend::BorrowFromSession(Wayland::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->wayland)
        impl_->wayland = std::make_unique<Wayland::WindowBackend>();
    if (!impl_->wayland->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::Wayland;
    return true;
}
#elif HONKORDGL_PLATFORM_NETBSD
bool WindowBackend::BorrowFromSession(NetBSD::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->netbsd)
        impl_->netbsd = std::make_unique<NetBSD::WindowBackend>();
    if (!impl_->netbsd->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::NetBSD;
    return true;
}
#elif HONKORDGL_PLATFORM_FREEBSD
bool WindowBackend::BorrowFromSession(FreeBSD::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->freebsd)
        impl_->freebsd = std::make_unique<FreeBSD::WindowBackend>();
    if (!impl_->freebsd->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::FreeBSD;
    return true;
}
#elif HONKORDGL_PLATFORM_RASPBERRY
bool WindowBackend::BorrowFromSession(RaspberryPi::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->raspberry)
        impl_->raspberry = std::make_unique<RaspberryPi::WindowBackend>();
    if (!impl_->raspberry->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::RaspberryPi;
    return true;
}
#elif HONKORDGL_PLATFORM_APPLE
bool WindowBackend::BorrowFromSession(Cocoa::Internal::PlatformSession& session) noexcept
{
    if (!impl_)
        impl_ = std::make_unique<Impl>();
    if (!impl_->cocoa)
        impl_->cocoa = std::make_unique<Cocoa::WindowBackend>();
    if (!impl_->cocoa->BorrowFromSession(session))
        return false;
    impl_->kind = WindowBackendKind::Cocoa;
    return true;
}
#endif
void WindowBackend::CloseDisplay() noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        impl_->x11->CloseDisplay();
    if (impl_->wayland)
        impl_->wayland->CloseDisplay();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        impl_->drm->CloseDisplay();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        impl_->netbsd->CloseDisplay();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        impl_->freebsd->CloseDisplay();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        impl_->raspberry->CloseDisplay();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        impl_->cocoa->CloseDisplay();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        impl_->win32->CloseDisplay();
#endif
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return false;
    bool ok = false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        ok = impl_->x11->CreateWindow(ctx);
    else if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        ok = impl_->wayland->CreateWindow(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    else if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        ok = impl_->drm->CreateWindow(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        ok = impl_->netbsd->CreateWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        ok = impl_->freebsd->CreateWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        ok = impl_->raspberry->CreateWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        ok = impl_->cocoa->CreateWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        ok = impl_->win32->CreateWindow(ctx);
#endif
    if (ok)
        (void)EnableRandrMonitorPolling();
    return ok;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        impl_->x11->DestroyWindow(ctx);
    if (impl_->wayland)
        impl_->wayland->DestroyWindow(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        impl_->drm->DestroyWindow(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        impl_->netbsd->DestroyWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        impl_->freebsd->DestroyWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        impl_->raspberry->DestroyWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        impl_->cocoa->DestroyWindow(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        impl_->win32->DestroyWindow(ctx);
#endif
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        return impl_->x11->ProcessEvents(ctx);
    if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        return impl_->wayland->ProcessEvents(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        return impl_->drm->ProcessEvents(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        return impl_->netbsd->ProcessEvents(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        return impl_->freebsd->ProcessEvents(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        return impl_->raspberry->ProcessEvents(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        return impl_->cocoa->ProcessEvents(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        return impl_->win32->ProcessEvents(ctx);
#endif
    return false;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        impl_->x11->SetWindowTitle(ctx, title);
    else if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        impl_->wayland->SetWindowTitle(ctx, title);
# if defined(HONKORDGL_ENABLE_DRM)
    else if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        impl_->drm->SetWindowTitle(ctx, title);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        impl_->netbsd->SetWindowTitle(ctx, title);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        impl_->freebsd->SetWindowTitle(ctx, title);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        impl_->raspberry->SetWindowTitle(ctx, title);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        impl_->cocoa->SetWindowTitle(ctx, title);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        impl_->win32->SetWindowTitle(ctx, title);
#endif
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        impl_->x11->ApplyWindowSettings(ctx);
    else if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        impl_->wayland->ApplyWindowSettings(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    else if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        impl_->drm->ApplyWindowSettings(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        impl_->netbsd->ApplyWindowSettings(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        impl_->freebsd->ApplyWindowSettings(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        impl_->raspberry->ApplyWindowSettings(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        impl_->cocoa->ApplyWindowSettings(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        impl_->win32->ApplyWindowSettings(ctx);
#endif
}
void WindowBackend::ResetApplicationSettings(ApplicationContextSettings& ctx, const ApplicationContextSettings& settings) noexcept
{
    if (!impl_)
        return;
    if (!ctx.window_handle) {
        ctx = settings;
        return;
    }

    const HonkordGL_GW_Handle keep_wh = ctx.window_handle;
    const HonkordGL_GW_Handle keep_dc = ctx.device_context;
    const HonkordGL_GW_Handle keep_dev = ctx.device;
    const HonkordGL_GW_Handle keep_surf = ctx.surface;
    const HonkordGL_GW_Handle keep_egl = ctx.egl_display;
    const HonkordGL_GW_Handle keep_priv = ctx.renderer_private;
    const int keep_plat = ctx.native_platform;
    const int keep_renderer = ctx.active_renderer;

    ctx = settings;
    ctx.window_handle = keep_wh;
    ctx.device_context = keep_dc;
    ctx.device = keep_dev;
    ctx.surface = keep_surf;
    ctx.egl_display = keep_egl;
    ctx.renderer_private = keep_priv;
    ctx.native_platform = keep_plat;
    ctx.active_renderer = keep_renderer;

    ApplyWindowSettings(ctx);
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        impl_->x11->AttachDisplayToContext(ctx);
    if (impl_->wayland)
        impl_->wayland->AttachDisplayToContext(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        impl_->drm->AttachDisplayToContext(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        impl_->netbsd->AttachDisplayToContext(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        impl_->freebsd->AttachDisplayToContext(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        impl_->raspberry->AttachDisplayToContext(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        impl_->cocoa->AttachDisplayToContext(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        impl_->win32->AttachDisplayToContext(ctx);
#endif
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        return impl_->x11->CreateIpcHelper(reinterpret_cast<X11::IpcHelperWindow *>(out), message_atom_name);
    if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        return impl_->wayland->CreateIpcHelper(reinterpret_cast<Wayland::IpcHelperWindow *>(out), message_atom_name);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        return impl_->drm->CreateIpcHelper(out, message_atom_name);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        return impl_->netbsd->CreateIpcHelper(reinterpret_cast<X11::IpcHelperWindow *>(out), message_atom_name);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        return impl_->freebsd->CreateIpcHelper(reinterpret_cast<X11::IpcHelperWindow *>(out), message_atom_name);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        return impl_->raspberry->CreateIpcHelper(reinterpret_cast<X11::IpcHelperWindow *>(out), message_atom_name);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        return impl_->cocoa->CreateIpcHelper(reinterpret_cast<Cocoa::IpcHelperWindow *>(out), message_atom_name);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        return impl_->win32->CreateIpcHelper(out, message_atom_name);
#endif
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    if (!impl_)
        return nullptr;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return reinterpret_cast<void *>(impl_->x11->GetDisplay());
    if (impl_->wayland)
        return reinterpret_cast<void *>(impl_->wayland->GetDisplay());
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->GetDisplay();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->GetDisplay();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->GetDisplay();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->GetDisplay();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->GetDisplay();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->GetDisplay();
#endif
    return nullptr;
}
int WindowBackend::GetDefaultScreen() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->GetDefaultScreen();
    if (impl_->wayland)
        return impl_->wayland->GetDefaultScreen();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->GetDefaultScreen();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->GetDefaultScreen();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->GetDefaultScreen();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->GetDefaultScreen();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->GetDefaultScreen();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->GetDefaultScreen();
#endif
    return 0;
}
bool WindowBackend::HasXkbExtension() const noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->HasXkbExtension();
    if (impl_->wayland)
        return impl_->wayland->HasXkbExtension();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->HasXkbExtension();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->HasXkbExtension();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->HasXkbExtension();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->HasXkbExtension();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->HasXkbExtension();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->HasXkbExtension();
#endif
    return false;
}
bool WindowBackend::HasXRandrExtension() const noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->HasXRandrExtension();
    if (impl_->wayland)
        return impl_->wayland->HasXRandrExtension();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->HasXRandrExtension();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->HasXRandrExtension();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->HasXRandrExtension();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->HasXRandrExtension();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->HasXRandrExtension();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->HasXRandrExtension();
#endif
    return false;
}
bool WindowBackend::HasXInput2Extension() const noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->HasXInput2Extension();
    if (impl_->wayland)
        return impl_->wayland->HasXInput2Extension();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->HasXInput2Extension();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->HasXInput2Extension();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->HasXInput2Extension();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->HasXInput2Extension();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->HasXInput2Extension();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->HasXInput2Extension();
#endif
    return false;
}
int WindowBackend::XInput2MajorVersion() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->XInput2MajorVersion();
    if (impl_->wayland)
        return impl_->wayland->XInput2MajorVersion();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->XInput2MajorVersion();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->XInput2MajorVersion();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->XInput2MajorVersion();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->XInput2MajorVersion();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->XInput2MajorVersion();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->XInput2MajorVersion();
#endif
    return 0;
}
int WindowBackend::XInput2MinorVersion() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->XInput2MinorVersion();
    if (impl_->wayland)
        return impl_->wayland->XInput2MinorVersion();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->XInput2MinorVersion();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->XInput2MinorVersion();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->XInput2MinorVersion();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->XInput2MinorVersion();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->XInput2MinorVersion();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->XInput2MinorVersion();
#endif
    return 0;
}
int WindowBackend::XInputOpcode() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->XInputOpcode();
    if (impl_->wayland)
        return impl_->wayland->XInputOpcode();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->XInputOpcode();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->XInputOpcode();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->XInputOpcode();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->XInputOpcode();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->XInputOpcode();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->XInputOpcode();
#endif
    return 0;
}
int WindowBackend::RandrEventBase() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->RandrEventBase();
    if (impl_->wayland)
        return impl_->wayland->RandrEventBase();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->RandrEventBase();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->RandrEventBase();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->RandrEventBase();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->RandrEventBase();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->RandrEventBase();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->RandrEventBase();
#endif
    return 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->EnableRandrMonitorPolling();
    if (impl_->wayland)
        return impl_->wayland->EnableRandrMonitorPolling();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->EnableRandrMonitorPolling();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->EnableRandrMonitorPolling();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->EnableRandrMonitorPolling();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->EnableRandrMonitorPolling();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->EnableRandrMonitorPolling();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->EnableRandrMonitorPolling();
#endif
    return false;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->PollMonitorsChanged();
    if (impl_->wayland)
        return impl_->wayland->PollMonitorsChanged();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->PollMonitorsChanged();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->PollMonitorsChanged();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->PollMonitorsChanged();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->PollMonitorsChanged();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->PollMonitorsChanged();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->PollMonitorsChanged();
#endif
    return false;
}
bool WindowBackend::PresentSoftwareFramebuffer(
    const ApplicationContextSettings& ctx,
    const std::uint8_t * rgba_pixels,
    int width,
    int height,
    int stride_bytes) noexcept
{
    if (!impl_)
        return false;
#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)
    if (NativePlatformUsesX11ClientHandles(static_cast<NativePlatform>(ctx.native_platform)))
        return PresentSoftwareFramebufferX11(ctx, rgba_pixels, width, height, stride_bytes);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        return impl_->win32->PresentSoftwareFramebuffer(ctx, rgba_pixels, width, height, stride_bytes);
#endif
    (void)ctx;
    (void)rgba_pixels;
    (void)width;
    (void)height;
    (void)stride_bytes;
    return false;
}

namespace {

bool ValidCursorImageParams(CursorImageParams const& p) noexcept
{
    if (!p.rgba_pixels || p.width <= 0 || p.height <= 0)
        return false;
    int const stride = p.stride_bytes > 0 ? p.stride_bytes : p.width * 4;
    if (stride < p.width * 4)
        return false;
    if (p.hotspot_x < 0 || p.hotspot_y < 0 || p.hotspot_x >= p.width || p.hotspot_y >= p.height)
        return false;
    int const dw = p.display_width > 0 ? p.display_width : p.width;
    int const dh = p.display_height > 0 ? p.display_height : p.height;
    if (dw <= 0 || dh <= 0)
        return false;
    return true;
}

} // namespace

void WindowBackend::SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        impl_->x11->SetCursorKind(ctx, kind);
    else if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        impl_->wayland->SetCursorKind(ctx, kind);
# if defined(HONKORDGL_ENABLE_DRM)
    else if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        impl_->drm->SetCursorKind(ctx, kind);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        impl_->netbsd->SetCursorKind(ctx, kind);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        impl_->freebsd->SetCursorKind(ctx, kind);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        impl_->raspberry->SetCursorKind(ctx, kind);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        impl_->cocoa->SetCursorKind(ctx, kind);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        impl_->win32->SetCursorKind(ctx, kind);
#endif
    (void)ctx;
    (void)kind;
}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept
{
    if (!ValidCursorImageParams(params))
        return false;
    if (!impl_)
        return false;
    bool ok = false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        ok = impl_->x11->SetCursorFromPixels(ctx, params);
    if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        ok = impl_->wayland->SetCursorFromPixels(ctx, params);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        ok = impl_->drm->SetCursorFromPixels(ctx, params);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        ok = impl_->netbsd->SetCursorFromPixels(ctx, params);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        ok = impl_->freebsd->SetCursorFromPixels(ctx, params);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        ok = impl_->raspberry->SetCursorFromPixels(ctx, params);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        ok = impl_->cocoa->SetCursorFromPixels(ctx, params);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        ok = impl_->win32->SetCursorFromPixels(ctx, params);
#endif
    if (ok)
        return true;
    SetCursorKind(ctx, CursorKind::Arrow);
    return true;
}
bool WindowBackend::SetCursorFromImageFile(
    ApplicationContextSettings& ctx,
    const char * path,
    const CursorImageParams& params) noexcept
{
    if (!path || !path[0])
        return false;
    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_uc * const data = stbi_load(path, &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0)
        return false;
    CursorImageParams p = params;
    p.rgba_pixels = data;
    p.width = w;
    p.height = h;
    p.stride_bytes = w * 4;
    const bool ok = SetCursorFromPixels(ctx, p);
    stbi_image_free(data);
    return ok;
}
void WindowBackend::ResetCursor(ApplicationContextSettings& ctx) noexcept
{
    if (!impl_)
        return;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->kind == WindowBackendKind::X11 && impl_->x11)
        impl_->x11->ResetCursor(ctx);
    else if (impl_->kind == WindowBackendKind::Wayland && impl_->wayland)
        impl_->wayland->ResetCursor(ctx);
# if defined(HONKORDGL_ENABLE_DRM)
    else if (impl_->kind == WindowBackendKind::DRM && impl_->drm)
        impl_->drm->ResetCursor(ctx);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->kind == WindowBackendKind::NetBSD && impl_->netbsd)
        impl_->netbsd->ResetCursor(ctx);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->kind == WindowBackendKind::FreeBSD && impl_->freebsd)
        impl_->freebsd->ResetCursor(ctx);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->kind == WindowBackendKind::RaspberryPi && impl_->raspberry)
        impl_->raspberry->ResetCursor(ctx);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->kind == WindowBackendKind::Cocoa && impl_->cocoa)
        impl_->cocoa->ResetCursor(ctx);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->kind == WindowBackendKind::Win32 && impl_->win32)
        impl_->win32->ResetCursor(ctx);
#endif
    (void)ctx;
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (!impl_)
        return 0;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->GetMonitorCount();
    if (impl_->wayland)
        return impl_->wayland->GetMonitorCount();
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->GetMonitorCount();
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->GetMonitorCount();
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->GetMonitorCount();
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->GetMonitorCount();
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->GetMonitorCount();
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->GetMonitorCount();
#endif
    return 0;
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!impl_)
        return false;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    if (impl_->x11)
        return impl_->x11->GetMonitorBounds(index, out);
    if (impl_->wayland)
        return impl_->wayland->GetMonitorBounds(index, out);
# if defined(HONKORDGL_ENABLE_DRM)
    if (impl_->drm)
        return impl_->drm->GetMonitorBounds(index, out);
# endif
#endif
#if HONKORDGL_PLATFORM_NETBSD
    if (impl_->netbsd)
        return impl_->netbsd->GetMonitorBounds(index, out);
#endif
#if HONKORDGL_PLATFORM_FREEBSD
    if (impl_->freebsd)
        return impl_->freebsd->GetMonitorBounds(index, out);
#endif
#if HONKORDGL_PLATFORM_RASPBERRY
    if (impl_->raspberry)
        return impl_->raspberry->GetMonitorBounds(index, out);
#endif
#if HONKORDGL_PLATFORM_APPLE
    if (impl_->cocoa)
        return impl_->cocoa->GetMonitorBounds(index, out);
#endif
#if HONKORDGL_PLATFORM_WINDOWS
    if (impl_->win32)
        return impl_->win32->GetMonitorBounds(index, out);
#endif
    return false;
}
void WindowBackend::SetTargetFrameRate(int fps) noexcept
{
    if (!impl_)
        return;
    impl_->target_frame_rate_fps = fps > 0 ? fps : 0;
    impl_->last_frame_tick = {};
}
void WindowBackend::DelayFrame() noexcept
{
    if (!impl_ || impl_->target_frame_rate_fps <= 0)
        return;
    using clock = std::chrono::steady_clock;
    auto const now = clock::now();
    auto const frame_budget = std::chrono::nanoseconds(1000000000LL / impl_->target_frame_rate_fps);
    if (impl_->last_frame_tick.time_since_epoch().count() != 0) {
        auto const elapsed = now - impl_->last_frame_tick;
        if (elapsed < frame_budget)
            std::this_thread::sleep_for(frame_budget - elapsed);
    }
    impl_->last_frame_tick = clock::now();
    ++impl_->frame_ticks;
}

std::uint64_t WindowBackend::GetTicks() const noexcept
{
    return impl_ ? impl_->frame_ticks : 0;
}

void WindowBackend::SetTicks(std::uint64_t ticks) noexcept
{
    if (!impl_)
        return;
    impl_->frame_ticks = ticks;
}

} // namespace HonkordGL::Graphics

#else /* no native WindowBackend in this build */

#include <chrono>
#include <thread>

namespace HonkordGL::Graphics {

int GetVideoDriverCount() noexcept
{
    return 0;
}

struct WindowBackend::Impl {
    WindowBackendKind kind{WindowBackendKind::Unknown};
    int target_frame_rate_fps{0};
    std::chrono::steady_clock::time_point last_frame_tick{};
    std::uint64_t frame_ticks{0};
};

WindowBackend::WindowBackend() noexcept
    : impl_(std::make_unique<Impl>())
{
}
WindowBackend::~WindowBackend() = default;
WindowBackend::WindowBackend(WindowBackend&& other) noexcept = default;
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept = default;

bool WindowBackend::Initialize(const char *) noexcept
{
    return false;
}
void WindowBackend::Shutdown() noexcept
{
    if (!impl_)
        return;
    impl_->kind = WindowBackendKind::Unknown;
    impl_->target_frame_rate_fps = 0;
    impl_->last_frame_tick = {};
    impl_->frame_ticks = 0;
}
WindowBackendKind WindowBackend::GetKind() const noexcept
{
    return impl_ ? impl_->kind : WindowBackendKind::Unknown;
}
bool WindowBackend::OpenDisplay(const char *) noexcept
{
    return false;
}
void WindowBackend::CloseDisplay() noexcept {}
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
void WindowBackend::ResetApplicationSettings(ApplicationContextSettings& ctx, const ApplicationContextSettings& settings) noexcept
{
    if (!ctx.window_handle) {
        ctx = settings;
        return;
    }
    const HonkordGL_GW_Handle keep_wh = ctx.window_handle;
    const HonkordGL_GW_Handle keep_dc = ctx.device_context;
    const HonkordGL_GW_Handle keep_dev = ctx.device;
    const HonkordGL_GW_Handle keep_surf = ctx.surface;
    const HonkordGL_GW_Handle keep_egl = ctx.egl_display;
    const HonkordGL_GW_Handle keep_priv = ctx.renderer_private;
    const int keep_plat = ctx.native_platform;
    const int keep_renderer = ctx.active_renderer;
    ctx = settings;
    ctx.window_handle = keep_wh;
    ctx.device_context = keep_dc;
    ctx.device = keep_dev;
    ctx.surface = keep_surf;
    ctx.egl_display = keep_egl;
    ctx.renderer_private = keep_priv;
    ctx.native_platform = keep_plat;
    ctx.active_renderer = keep_renderer;
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings&) const noexcept {}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow *, const char *) const noexcept
{
    return false;
}
void * WindowBackend::GetDisplay() const noexcept
{
    return nullptr;
}
int WindowBackend::GetDefaultScreen() const noexcept
{
    return 0;
}
bool WindowBackend::HasXkbExtension() const noexcept
{
    return false;
}
bool WindowBackend::HasXRandrExtension() const noexcept
{
    return false;
}
bool WindowBackend::HasXInput2Extension() const noexcept
{
    return false;
}
int WindowBackend::XInput2MajorVersion() const noexcept
{
    return 0;
}
int WindowBackend::XInput2MinorVersion() const noexcept
{
    return 0;
}
int WindowBackend::XInputOpcode() const noexcept
{
    return 0;
}
int WindowBackend::RandrEventBase() const noexcept
{
    return 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    return false;
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    return false;
}
bool WindowBackend::PresentSoftwareFramebuffer(
    const ApplicationContextSettings&,
    const std::uint8_t *,
    int,
    int,
    int) noexcept
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
void WindowBackend::SetCursorKind(ApplicationContextSettings&, CursorKind) noexcept {}
bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings&, const CursorImageParams& params) noexcept
{
    if (!params.rgba_pixels || params.width <= 0 || params.height <= 0)
        return false;
    int const stride = params.stride_bytes > 0 ? params.stride_bytes : params.width * 4;
    if (stride < params.width * 4)
        return false;
    if (params.hotspot_x < 0 || params.hotspot_y < 0 || params.hotspot_x >= params.width
        || params.hotspot_y >= params.height)
        return false;
    return true;
}
bool WindowBackend::SetCursorFromImageFile(ApplicationContextSettings&, const char *, const CursorImageParams&) noexcept
{
    return false;
}
void WindowBackend::ResetCursor(ApplicationContextSettings&) noexcept {}

void WindowBackend::SetTargetFrameRate(int fps) noexcept
{
    if (!impl_)
        return;
    impl_->target_frame_rate_fps = fps > 0 ? fps : 0;
    impl_->last_frame_tick = {};
}

void WindowBackend::DelayFrame() noexcept
{
    if (!impl_ || impl_->target_frame_rate_fps <= 0)
        return;
    using clock = std::chrono::steady_clock;
    auto const now = clock::now();
    auto const frame_budget = std::chrono::nanoseconds(1000000000LL / impl_->target_frame_rate_fps);
    if (impl_->last_frame_tick.time_since_epoch().count() != 0) {
        auto const elapsed = now - impl_->last_frame_tick;
        if (elapsed < frame_budget)
            std::this_thread::sleep_for(frame_budget - elapsed);
    }
    impl_->last_frame_tick = clock::now();
    ++impl_->frame_ticks;
}

std::uint64_t WindowBackend::GetTicks() const noexcept
{
    return impl_ ? impl_->frame_ticks : 0;
}

void WindowBackend::SetTicks(std::uint64_t ticks) noexcept
{
    if (!impl_)
        return;
    impl_->frame_ticks = ticks;
}

#endif