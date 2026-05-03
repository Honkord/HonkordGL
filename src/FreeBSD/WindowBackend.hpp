/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — FreeBSD / X11 window backend
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_FREEBSD_WINDOWBACKEND_HPP
#define HONKORDGL_FREEBSD_WINDOWBACKEND_HPP

#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/X11IpcWindow.h>

#include "X11/CursorX11.hpp"
#include <X11/Xlib.h>

namespace HonkordGL::Graphics::FreeBSD::Internal {
class PlatformSession;
}

namespace HonkordGL::Graphics::FreeBSD {

using IpcHelperWindow = X11::IpcHelperWindow;

/**
 * X11 binding for `ApplicationContextSettings` on FreeBSD (`NativePlatform::FreeBSD`).
 *
 * Handle convention (same as X11):
 * - window_handle: cast the X Window ID with `(void *)(uintptr_t)win`
 * - device_context: `Display *` after `AttachDisplayToContext` / `CreateWindow`
 */
class WindowBackend {
public:
    WindowBackend() noexcept = default;
    ~WindowBackend();

    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;
    WindowBackend(WindowBackend&& other) noexcept;
    WindowBackend& operator=(WindowBackend&& other) noexcept;

    HONKORDGL_ND bool OpenDisplay(const char * displayName = nullptr) noexcept;
    /**
     * Use the display from a bootstrapped `FreeBSD::Internal::PlatformSession` (does not take ownership).
     */
    HONKORDGL_ND bool BorrowFromSession(Internal::PlatformSession& session) noexcept;
    void CloseDisplay() noexcept;

    HONKORDGL_ND bool CreateWindow(ApplicationContextSettings& ctx) noexcept;

    void DestroyWindow(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND bool ProcessEvents(ApplicationContextSettings& ctx) noexcept;

    void SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept;

    void ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept;

    void AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept;

    HONKORDGL_ND bool CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept;

    HONKORDGL_ND Display * GetDisplay() const noexcept { return display_; }
    HONKORDGL_ND int GetDefaultScreen() const noexcept { return screen_; }

    HONKORDGL_ND bool HasXkbExtension() const noexcept { return ext_xkb_; }
    HONKORDGL_ND bool HasXRandrExtension() const noexcept { return ext_xrandr_; }
    HONKORDGL_ND bool HasXInput2Extension() const noexcept { return ext_xinput2_; }
    HONKORDGL_ND int XInput2MajorVersion() const noexcept { return xi2_major_; }
    HONKORDGL_ND int XInput2MinorVersion() const noexcept { return xi2_minor_; }
    HONKORDGL_ND int XInputOpcode() const noexcept { return xi_opcode_; }

    HONKORDGL_ND int RandrEventBase() const noexcept { return xrandr_event_base_; }

    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;

    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    HONKORDGL_ND int GetMonitorCount() const noexcept;
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

private:
    void ensure_wm_delete_atom() noexcept;
    void initializeX11Extensions() noexcept;
    static ::Window NativeWindow(const ApplicationContextSettings& ctx) noexcept;

    Display * display_{nullptr};
    int screen_{0};
    bool owns_display_{false};
    Atom wm_delete_window_{None};

    bool ext_xkb_{false};
    bool ext_xrandr_{false};
    bool ext_xinput2_{false};
    int xi_opcode_{0};
    int xi2_major_{0};
    int xi2_minor_{0};
    int xrandr_major_{0};
    int xrandr_minor_{0};
    int xrandr_event_base_{0};
    int xrandr_error_base_{0};

    X11::Internal::CursorX11State cursor_state_;
};

} // namespace HonkordGL::Graphics::FreeBSD

#endif