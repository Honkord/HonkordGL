/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Linux KMS/DRM window backend (libdrm)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_DRM_WINDOWBACKEND_HPP
#define HONKORDGL_DRM_WINDOWBACKEND_HPP

#include <HonkordGL/WindowBackend.h>

namespace HonkordGL::Graphics::DRM::Internal {
struct DrmWindowState;
}

namespace HonkordGL::Graphics::DRM {

/**
 * Direct scanout via DRM dumb buffer + drmModeSetCrtc (no X11/Wayland).
 *
 * - Build with `-DHONKORDGL_ENABLE_DRM` and link `-ldrm`.
 * - `Initialize()` tries this driver after X11 and Wayland when enabled.
 * - `OpenDisplay(path)`: nullptr or "" → `/dev/dri/card0`, else the device path.
 * - `GetDisplay()` / `device_context`: pointer to an internal struct whose first member is `int fd` (DRM fd).
 * - `window_handle`: internal `DrmWindowState *`.
 * - Input: not wired; use libinput separately if needed.
 */
class WindowBackend {
public:
    WindowBackend() noexcept = default;
    ~WindowBackend();

    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;
    WindowBackend(WindowBackend&& other) noexcept;
    WindowBackend& operator=(WindowBackend&& other) noexcept;

    HONKORDGL_ND bool OpenDisplay(const char * device_path = nullptr) noexcept;
    void CloseDisplay() noexcept;

    HONKORDGL_ND bool CreateWindow(ApplicationContextSettings& ctx) noexcept;
    void DestroyWindow(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND bool ProcessEvents(ApplicationContextSettings& ctx) noexcept;

    void SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept;
    void ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept;
    void AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept;

    HONKORDGL_ND bool CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept;

    HONKORDGL_ND void * GetDisplay() const noexcept;
    HONKORDGL_ND int GetDefaultScreen() const noexcept { return 0; }

    HONKORDGL_ND bool HasXkbExtension() const noexcept { return false; }
    HONKORDGL_ND bool HasXRandrExtension() const noexcept { return false; }
    HONKORDGL_ND bool HasXInput2Extension() const noexcept { return false; }
    HONKORDGL_ND int XInput2MajorVersion() const noexcept { return 0; }
    HONKORDGL_ND int XInput2MinorVersion() const noexcept { return 0; }
    HONKORDGL_ND int XInputOpcode() const noexcept { return 0; }
    HONKORDGL_ND int RandrEventBase() const noexcept { return 0; }

    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;
    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    HONKORDGL_ND int GetMonitorCount() const noexcept;
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

private:
    int drm_fd_{-1};
    struct DrmDisplayTag {
        int fd{-1};
    } display_tag_{};
    Internal::DrmWindowState * active_{nullptr};
};

} // namespace HonkordGL::Graphics::DRM

#endif