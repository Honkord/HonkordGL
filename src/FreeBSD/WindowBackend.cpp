/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — FreeBSD / X11 window backend
 * Copyright (C) 2026 Honkord
 */

#include "WindowBackend.hpp"
#include "PlatformSession.hpp"

#include "X11/MonitorsX11.hpp"
#include "X11/RandrMonitorPoll.hpp"

#include <HonkordGL/Events.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#include <cstdint>
#include <cstring>
#include <utility>

#include <HonkordGL/Video.h>

#if defined(__FreeBSD__)

namespace HonkordGL::Graphics::FreeBSD {

namespace {

constexpr int kDefaultDpi = 96;

} // namespace

WindowBackend::~WindowBackend()
{
    CloseDisplay();
}

WindowBackend::WindowBackend(WindowBackend&& other) noexcept
    : display_(other.display_),
      screen_(other.screen_),
      owns_display_(other.owns_display_),
      wm_delete_window_(other.wm_delete_window_),
      ext_xkb_(other.ext_xkb_),
      ext_xrandr_(other.ext_xrandr_),
      ext_xinput2_(other.ext_xinput2_),
      xi_opcode_(other.xi_opcode_),
      xi2_major_(other.xi2_major_),
      xi2_minor_(other.xi2_minor_),
      xrandr_major_(other.xrandr_major_),
      xrandr_minor_(other.xrandr_minor_),
      xrandr_event_base_(other.xrandr_event_base_),
      xrandr_error_base_(other.xrandr_error_base_),
      cursor_state_(std::move(other.cursor_state_))
{
    other.display_ = nullptr;
    other.screen_ = 0;
    other.owns_display_ = false;
    other.wm_delete_window_ = None;
    other.ext_xkb_ = false;
    other.ext_xrandr_ = false;
    other.ext_xinput2_ = false;
    other.xi_opcode_ = 0;
    other.xi2_major_ = other.xi2_minor_ = 0;
    other.xrandr_major_ = other.xrandr_minor_ = 0;
    other.xrandr_event_base_ = other.xrandr_error_base_ = 0;
}
WindowBackend& WindowBackend::operator=(WindowBackend&& other) noexcept
{
    if (this != &other) {
        CloseDisplay();
        display_ = other.display_;
        screen_ = other.screen_;
        owns_display_ = other.owns_display_;
        wm_delete_window_ = other.wm_delete_window_;
        ext_xkb_ = other.ext_xkb_;
        ext_xrandr_ = other.ext_xrandr_;
        ext_xinput2_ = other.ext_xinput2_;
        xi_opcode_ = other.xi_opcode_;
        xi2_major_ = other.xi2_major_;
        xi2_minor_ = other.xi2_minor_;
        xrandr_major_ = other.xrandr_major_;
        xrandr_minor_ = other.xrandr_minor_;
        xrandr_event_base_ = other.xrandr_event_base_;
        xrandr_error_base_ = other.xrandr_error_base_;
        cursor_state_ = std::move(other.cursor_state_);
        other.display_ = nullptr;
        other.screen_ = 0;
        other.owns_display_ = false;
        other.wm_delete_window_ = None;
        other.ext_xkb_ = false;
        other.ext_xrandr_ = false;
        other.ext_xinput2_ = false;
        other.xi_opcode_ = 0;
        other.xi2_major_ = other.xi2_minor_ = 0;
        other.xrandr_major_ = other.xrandr_minor_ = 0;
        other.xrandr_event_base_ = other.xrandr_error_base_ = 0;
    }
    return *this;
}
void WindowBackend::initializeX11Extensions() noexcept
{
    ext_xkb_ = false;
    ext_xrandr_ = false;
    ext_xinput2_ = false;
    xi_opcode_ = 0;
    xi2_major_ = xi2_minor_ = 0;
    xrandr_major_ = xrandr_minor_ = 0;
    xrandr_event_base_ = 0;
    xrandr_error_base_ = 0;

    if (!display_)
        return;

    {
        int xkb_opcode = 0;
        int xkb_ev = 0;
        int xkb_err = 0;
        int xkb_maj = 0;
        int xkb_min = 0;
        if (XkbQueryExtension(display_, &xkb_opcode, &xkb_ev, &xkb_err, &xkb_maj, &xkb_min)) {
            ext_xkb_ = true;
            Bool supported = False;
            (void)XkbSetDetectableAutoRepeat(display_, True, &supported);
        }
    }

    if (XRRQueryExtension(display_, &xrandr_event_base_, &xrandr_error_base_)) {
        ext_xrandr_ = true;
        (void)XRRQueryVersion(display_, &xrandr_major_, &xrandr_minor_);
    }

    {
        int xi_event = 0;
        int xi_error = 0;
        if (XQueryExtension(display_, "XInputExtension", &xi_opcode_, &xi_event, &xi_error)) {
            int maj = 2;
            int min = 0;
            if (XIQueryVersion(display_, &maj, &min) == Success && maj >= 2) {
                ext_xinput2_ = true;
                xi2_major_ = maj;
                xi2_minor_ = min;
            }
        }
    }
}
bool WindowBackend::OpenDisplay(const char * displayName) noexcept
{
    if (display_)
        return true;
    display_ = XOpenDisplay(displayName);
    if (!display_) {
        HonkordGL::Graphics::SetInternalApiError("XOpenDisplay failed (check DISPLAY and X server).");
        return false;
    }
    screen_ = DefaultScreen(display_);
    owns_display_ = true;
    initializeX11Extensions();
    return true;
}
bool WindowBackend::BorrowFromSession(Internal::PlatformSession& session) noexcept
{
    if (!session.is_connected()) {
        HonkordGL::Graphics::SetInternalApiError("BorrowFromSession: FreeBSD PlatformSession is not connected.");
        return false;
    }
    CloseDisplay();
    display_ = session.display();
    screen_ = session.default_screen();
    owns_display_ = false;
    wm_delete_window_ = session.wm_delete_window_atom();
    initializeX11Extensions();
    return true;
}
void WindowBackend::CloseDisplay() noexcept
{
    if (display_) {
        cursor_state_.destroyAll(display_);
        if (owns_display_)
            XCloseDisplay(display_);
        display_ = nullptr;
        screen_ = 0;
        owns_display_ = false;
    }
    wm_delete_window_ = None;
    ext_xkb_ = false;
    ext_xrandr_ = false;
    ext_xinput2_ = false;
    xi_opcode_ = 0;
    xi2_major_ = xi2_minor_ = 0;
    xrandr_major_ = xrandr_minor_ = 0;
    xrandr_event_base_ = 0;
    xrandr_error_base_ = 0;
}
bool WindowBackend::EnableRandrMonitorPolling() noexcept
{
    if (!display_ || !ext_xrandr_)
        return false;
    return X11::Internal::RandrEnableMonitorPolling(display_, screen_);
}
bool WindowBackend::PollMonitorsChanged() noexcept
{
    if (!display_ || !ext_xrandr_)
        return false;
    return X11::Internal::RandrPollMonitorTopologyChanged(display_, screen_, xrandr_event_base_);
}
int WindowBackend::GetMonitorCount() const noexcept
{
    if (!display_)
        return 0;
    return X11::Internal::GetMonitorCount(display_, screen_);
}
bool WindowBackend::GetMonitorBounds(int index, Recti * out) const noexcept
{
    if (!display_ || !out)
        return false;
    return X11::Internal::GetMonitorBounds(display_, screen_, index, out);
}
void WindowBackend::ensure_wm_delete_atom() noexcept
{
    if (!display_ || wm_delete_window_ != None)
        return;
    wm_delete_window_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
}
::Window WindowBackend::NativeWindow(const ApplicationContextSettings& ctx) noexcept
{
    return static_cast<::Window>(reinterpret_cast<std::uintptr_t>(ctx.window_handle));
}
bool WindowBackend::CreateWindow(ApplicationContextSettings& ctx) noexcept
{
    const char * const title = ctx.window_title;
    const int x = ctx.client_rect.x;
    const int y = ctx.client_rect.y;
    const unsigned width = static_cast<unsigned>(ctx.client_rect.w);
    const unsigned height = static_cast<unsigned>(ctx.client_rect.z);

    if (!display_ || width == 0 || height == 0) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: invalid display or zero size.");
        return false;
    }

    ensure_wm_delete_atom();

    const ::Window root = RootWindow(display_, screen_);
    const unsigned long black = BlackPixel(display_, screen_);
    const unsigned long white = WhitePixel(display_, screen_);

    const ::Window win = XCreateSimpleWindow(
        display_,
        root,
        x,
        y,
        width,
        height,
        0,
        black,
        white);

    if (!win) {
        HonkordGL::Graphics::SetInternalApiError("CreateWindow: XCreateSimpleWindow failed.");
        return false;
    }

    XSelectInput(
        display_,
        win,
        StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask |
        EnterWindowMask | LeaveWindowMask | VisibilityChangeMask);

    if (title && std::strlen(title) > 0)
        XStoreName(display_, win, title);

    if (wm_delete_window_ != None) {
        XSetWMProtocols(display_, win, &wm_delete_window_, 1);
    }

    XMapWindow(display_, win);
    XFlush(display_);

    ctx.window_handle = reinterpret_cast<HonkordGL_GW_Handle>(static_cast<std::uintptr_t>(win));
    ctx.client_rect.x = x;
    ctx.client_rect.y = y;
    ctx.client_rect.w = static_cast<int>(width);
    ctx.client_rect.z = static_cast<int>(height);
    ctx.is_visible = 1;
    ctx.is_minimized = 0;
    ctx.dpi_x = kDefaultDpi;
    ctx.dpi_y = kDefaultDpi;
    ctx.frame_buffer_width = static_cast<int>(width);
    ctx.frame_buffer_height = static_cast<int>(height);
    ctx.needs_resize = 0;
    ctx.native_platform = static_cast<int>(NativePlatform::FreeBSD);
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(display_);
    return true;
}
void WindowBackend::DestroyWindow(ApplicationContextSettings& ctx) noexcept
{
    if (!display_ || !ctx.window_handle)
        return;
    const ::Window win = NativeWindow(ctx);
    cursor_state_.onDestroyWindow(display_, win);
    XDestroyWindow(display_, win);
    XFlush(display_);
    ctx.window_handle = nullptr;
    if (ctx.device_context == reinterpret_cast<HonkordGL_GW_Handle>(display_))
        ctx.device_context = nullptr;
}
bool WindowBackend::ProcessEvents(ApplicationContextSettings& ctx) noexcept
{
    if (!display_ || !ctx.window_handle)
        return false;

    if (!ctx.device_context)
        ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(display_);

    HonkordGL::Events::Event e{};
    while (HonkordGL::Events::PollEvent(ctx, e)) {
        if (e.type == HonkordGL::Events::EventType::QUIT)
            return false;
    }
    return true;
}
void WindowBackend::SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept
{
    if (!display_ || !ctx.window_handle || !title)
        return;
    XStoreName(display_, NativeWindow(ctx), title);
    XFlush(display_);
}
void WindowBackend::ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept
{
    if (!display_ || !ctx.window_handle)
        return;
    const ::Window win = NativeWindow(ctx);
    if (ctx.window_title && std::strlen(ctx.window_title) > 0)
        XStoreName(display_, win, ctx.window_title);
    const int w = ctx.client_rect.w;
    const int h = ctx.client_rect.z;
    if (w > 0 && h > 0) {
        XMoveResizeWindow(
            display_,
            win,
            ctx.client_rect.x,
            ctx.client_rect.y,
            static_cast<unsigned>(w),
            static_cast<unsigned>(h));
        ctx.frame_buffer_width = w;
        ctx.frame_buffer_height = h;
    }
    XFlush(display_);
}
void WindowBackend::AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept
{
    ctx.device_context = reinterpret_cast<HonkordGL_GW_Handle>(display_);
}
bool WindowBackend::CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept
{
    if (!display_ || !out)
        return false;
    if (!X11::CreateIpcHelperWindow(reinterpret_cast<HonkordGL_GW_Handle>(display_), screen_, message_atom_name, out)) {
        HonkordGL::Graphics::SetInternalApiError("CreateIpcHelperWindow failed.");
        return false;
    }
    return true;
}

void WindowBackend::SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept
{
    if (!display_ || !ctx.window_handle)
        return;
    cursor_state_.setKind(display_, NativeWindow(ctx), kind);
    XFlush(display_);
}

bool WindowBackend::SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept
{
    if (!display_ || !ctx.window_handle)
        return false;
    const bool ok = cursor_state_.setFromPixels(display_, NativeWindow(ctx), params);
    if (ok)
        XFlush(display_);
    return ok;
}

void WindowBackend::ResetCursor(ApplicationContextSettings& ctx) noexcept
{
    if (!display_ || !ctx.window_handle)
        return;
    cursor_state_.reset(display_, NativeWindow(ctx));
    XFlush(display_);
}

} // namespace HonkordGL::Graphics::FreeBSD

#else

namespace HonkordGL::Graphics::FreeBSD {
void FreeBSDWindowBackendCompileUnitPlaceholder() noexcept {}
}

#endif
