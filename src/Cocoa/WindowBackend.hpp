/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa / AppKit window backend
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_WINDOWBACKEND_HPP
#define HONKORDGL_COCOA_WINDOWBACKEND_HPP

#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/CocoaIpcWindow.h>

#include <unordered_map>

namespace HonkordGL::Graphics::Cocoa::Internal {
class PlatformSession;
}

namespace HonkordGL::Graphics::Cocoa {

/**
 * AppKit binding for ApplicationContextSettings.
 *
 * Handle convention:
 * - window_handle: NSWindow * (cast via reinterpret_cast)
 * - device_context: optional; AttachDisplayToContext() stores NSApplication * for APIs that read handles
 */
class WindowBackend {
public:
    WindowBackend() noexcept = default;
    ~WindowBackend();

    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;
    WindowBackend(WindowBackend&& other) noexcept;
    WindowBackend& operator=(WindowBackend&& other) noexcept;

    HONKORDGL_ND bool OpenDisplay(const char * app_name = nullptr) noexcept;
    /**
     * Use the application from a bootstrapped Internal::PlatformSession (does not take ownership).
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

    /** NSApplication * as void *. */
    HONKORDGL_ND void * GetDisplay() const noexcept;
    HONKORDGL_ND int GetDefaultScreen() const noexcept { return screen_; }

    /** After OpenDisplay: keyboard (Text Input / Carbon), Quartz display config, no XI2. */
    HONKORDGL_ND bool HasXkbExtension() const noexcept { return ext_xkb_; }
    HONKORDGL_ND bool HasXRandrExtension() const noexcept { return ext_xrandr_; }
    HONKORDGL_ND bool HasXInput2Extension() const noexcept { return ext_xinput2_; }
    HONKORDGL_ND int XInput2MajorVersion() const noexcept { return xi2_major_; }
    HONKORDGL_ND int XInput2MinorVersion() const noexcept { return xi2_minor_; }
    HONKORDGL_ND int XInputOpcode() const noexcept { return xi_opcode_; }

    /** Not applicable on Cocoa (always 0). */
    HONKORDGL_ND int RandrEventBase() const noexcept { return xrandr_event_base_; }

    /** Registers for screen-parameter change notifications (Quartz). No-op if unavailable. */
    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;

    /**
     * Non-blocking: true if the set of active Core Graphics displays changed since the last poll or enable.
     */
    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    /** Number of connected monitors (0 if display not open). */
    HONKORDGL_ND int GetMonitorCount() const noexcept;
    /**
     * Global pixel bounds for monitor `index` (top-left origin, y increases downward).
     */
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

private:
    void initializeCocoaExtensions() noexcept;
    static void * NativeWindow(const ApplicationContextSettings& ctx) noexcept;

    void releaseCocoaCursor(void * winKey) noexcept;
    void clearCocoaCursors() noexcept;

    std::unordered_map<void *, void *> cocoa_cursors_;

    bool display_open_{false};
    bool owns_display_{false};
    int screen_{0};
    bool monitor_poll_enabled_{false};

    bool ext_xkb_{false};
    bool ext_xrandr_{false};
    bool ext_xinput2_{false};
    int xi_opcode_{0};
    int xi2_major_{0};
    int xi2_minor_{0};
    int xrandr_event_base_{0};
    int xrandr_error_base_{0};
};

} // namespace HonkordGL::Graphics::Cocoa

#endif