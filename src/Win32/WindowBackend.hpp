/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Win32 window backend
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_WIN32_WINDOWBACKEND_HPP
#define HONKORDGL_WIN32_WINDOWBACKEND_HPP

#include <HonkordGL/WindowBackend.h>

#include <HonkordGL/Cursor.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

// Prevent Win32 macros from colliding with portable API identifiers.
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateWindowW
#undef CreateWindowW
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace HonkordGL::Graphics::Win32 {

/**
 * Win32 binding for ApplicationContextSettings.
 *
 * Handles:
 * - window_handle: Internal::Win32WindowState* (opaque)
 * - device_context: HINSTANCE from GetModuleHandle(nullptr)
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
    HONKORDGL_ND bool PresentSoftwareFramebuffer(
        ApplicationContextSettings const& ctx,
        std::uint8_t const * rgba_pixels,
        int width,
        int height,
        int stride_bytes) noexcept;

    HONKORDGL_ND int GetMonitorCount() const noexcept;
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

private:
    bool ensure_class_registered() noexcept;

    HINSTANCE hinstance_{nullptr};
    bool display_open_{false};
    bool class_registered_{false};
    int last_monitor_count_{0};
};

} // namespace HonkordGL::Graphics::Win32

#endif