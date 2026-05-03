/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 window backend
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_WINDOWBACKEND_HPP
#define HONKORDGL_X11_WINDOWBACKEND_HPP

#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/X11IpcWindow.h>

#include "CursorX11.hpp"
#include <X11/Xlib.h>

namespace HonkordGL::Graphics::X11::Internal {
class PlatformSession;
}

namespace HonkordGL::Graphics::X11 {

/**
 * X11 binding for ApplicationContextSettings.
 *
 * Handle convention for this backend:
 * - window_handle: cast the X Window ID with (void *)(uintptr_t)win
 * - device_context: optional; call AttachDisplayToContext() to store Display * for APIs that only read handles
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
     * Use the display from a bootstrapped Internal::PlatformSession (does not take ownership).
     * Call after session.connect_display (and usually session.setup_core_x11). Closes any prior owned display.
     */
    HONKORDGL_ND bool BorrowFromSession(Internal::PlatformSession& session) noexcept;
    void CloseDisplay() noexcept;

    /**
     * Creates a mapped, decorated top-level window and fills ctx.
     * Reads ctx.window_title and ctx.client_rect (x, y, width in w, height in z).
     * Rendering handles (device, surface) are left untouched for GL/Vulkan setup elsewhere.
     */
    HONKORDGL_ND bool CreateWindow(ApplicationContextSettings& ctx) noexcept;

    void DestroyWindow(ApplicationContextSettings& ctx) noexcept;

    /** Non-blocking drain of the display queue. Returns false if the user closed the window. */
    HONKORDGL_ND bool ProcessEvents(ApplicationContextSettings& ctx) noexcept;

    void SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept;

    void ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept;

    void AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept;

    /** Same as CreateIpcHelperWindow(GetDisplay(), GetDefaultScreen(), …). */
    HONKORDGL_ND bool CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept;

    HONKORDGL_ND Display * GetDisplay() const noexcept { return display_; }
    HONKORDGL_ND int GetDefaultScreen() const noexcept { return screen_; }

    /** Present after OpenDisplay: XKEYBOARD, RandR, X Input 2. */
    HONKORDGL_ND bool HasXkbExtension() const noexcept { return ext_xkb_; }
    HONKORDGL_ND bool HasXRandrExtension() const noexcept { return ext_xrandr_; }
    HONKORDGL_ND bool HasXInput2Extension() const noexcept { return ext_xinput2_; }
    HONKORDGL_ND int XInput2MajorVersion() const noexcept { return xi2_major_; }
    HONKORDGL_ND int XInput2MinorVersion() const noexcept { return xi2_minor_; }
    HONKORDGL_ND int XInputOpcode() const noexcept { return xi_opcode_; }

    /** RandR event base from XRRQueryExtension (undefined if !HasXRandrExtension()). */
    HONKORDGL_ND int RandrEventBase() const noexcept { return xrandr_event_base_; }

    /**
     * Request RandR output / CRTC / screen notifications on the root window (same connection).
     * No-op if RandR is not present.
     */
    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;

    /**
     * Non-blocking: true if the set of connected RandR outputs changed since the last poll or enable,
     * or matching RandR events were drained (plug, unplug, mode set, etc.).
     * Call from your frame loop; pair with XRRGetScreenResources / XRRGetMonitors to refresh layout.
     */
    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    /** Number of connected monitors (0 if display not open). */
    HONKORDGL_ND int GetMonitorCount() const noexcept;
    /**
     * Global pixel bounds for monitor `index` (top-left origin, y increases downward).
     * Matches X11 RandR / Cocoa NSScreen order for the active desktop.
     */
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

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

    Internal::CursorX11State cursor_state_;
};

} // namespace HonkordGL::Graphics::X11

#endif