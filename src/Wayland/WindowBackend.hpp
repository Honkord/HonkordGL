/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland window backend
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_WAYLAND_WINDOWBACKEND_HPP
#define HONKORDGL_WAYLAND_WINDOWBACKEND_HPP

#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WaylandIpcWindow.h>

#include <memory>

struct wl_display;

namespace HonkordGL::Graphics::Wayland::Internal {
class PlatformSession;
}

namespace HonkordGL::Graphics::Wayland {

namespace detail {
struct WaylandBackendState;
}

/**
 * Wayland binding for ApplicationContextSettings.
 *
 * Handle convention:
 * - window_handle: Internal::WaylandWindowContext* (opaque; created by this backend)
 * - device_context: wl_display*
 */
class WindowBackend {
public:
    WindowBackend() noexcept;
    ~WindowBackend();

    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;
    WindowBackend(WindowBackend&& other) noexcept;
    WindowBackend& operator=(WindowBackend&& other) noexcept;

    HONKORDGL_ND bool OpenDisplay(const char* name = nullptr) noexcept;
    HONKORDGL_ND bool BorrowFromSession(Internal::PlatformSession& session) noexcept;
    void CloseDisplay() noexcept;

    HONKORDGL_ND bool CreateWindow(ApplicationContextSettings& ctx) noexcept;

    void DestroyWindow(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND bool ProcessEvents(ApplicationContextSettings& ctx) noexcept;

    void SetWindowTitle(ApplicationContextSettings& ctx, const char* title) noexcept;

    void ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept;

    void AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept;

    HONKORDGL_ND bool CreateIpcHelper(IpcHelperWindow* out, const char* message_atom_name) const noexcept;

    HONKORDGL_ND wl_display* GetDisplay() const noexcept;
    HONKORDGL_ND int GetDefaultScreen() const noexcept { return 0; }

    HONKORDGL_ND bool HasXkbExtension() const noexcept;
    HONKORDGL_ND bool HasXRandrExtension() const noexcept;
    HONKORDGL_ND bool HasXInput2Extension() const noexcept;
    HONKORDGL_ND int XInput2MajorVersion() const noexcept;
    HONKORDGL_ND int XInput2MinorVersion() const noexcept;
    HONKORDGL_ND int XInputOpcode() const noexcept;

    HONKORDGL_ND int RandrEventBase() const noexcept;

    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;
    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    HONKORDGL_ND int GetMonitorCount() const noexcept;
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti* out) const noexcept;

    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

private:
    std::unique_ptr<detail::WaylandBackendState> impl_;
};

} // namespace HonkordGL::Graphics::Wayland

#endif