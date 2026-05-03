/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — cross-platform window backend (portable API)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_GRAPHICS_WINDOWBACKEND_H
#define HONKORDGL_GRAPHICS_WINDOWBACKEND_H

#include <HonkordGL/Config.h>
#include <HonkordGL/Cursor.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdint>
#include <memory>

namespace HonkordGL::Graphics::X11::Internal {
class PlatformSession;
}
namespace HonkordGL::Graphics::Wayland::Internal {
class PlatformSession;
}
namespace HonkordGL::Graphics::Cocoa::Internal {
class PlatformSession;
}
namespace HonkordGL::Graphics::NetBSD::Internal {
class PlatformSession;
}
namespace HonkordGL::Graphics::FreeBSD::Internal {
class PlatformSession;
}
namespace HonkordGL::Graphics::RaspberryPi::Internal {
class PlatformSession;
}

#if HONKORDGL_PLATFORM_ANDROID
struct android_app;
#endif

namespace HonkordGL::Graphics {

/** Active platform driver selected by Initialize() (Unknown = no driver yet / after Shutdown). */
enum class WindowBackendKind : unsigned char {
    Unknown = 0,
    X11,
    Wayland,
    Cocoa,
    Win32,
    Android,
    DRM,
    NetBSD,
    FreeBSD,
    RaspberryPi,
};

/**
 * Number of built-in display drivers in this build (the drivers Initialize() tries in order).
 * Desktop Linux: 2 (X11, Wayland), or 3 when built with `HONKORDGL_ENABLE_DRM` (adds KMS/DRM last). NetBSD / FreeBSD: 1 each (X11 via the BSD-specific driver). Raspberry Pi: 1 when built with `HONKORDGL_PLATFORM_RASPBERRY_PI` (X11-only driver). macOS: 1 (Cocoa). Windows: 1 (Win32). Android: 1. Other targets: 0.
 */
HONKORDGL_ND int GetVideoDriverCount() noexcept;

/**
 * IPC helper layout (binary-compatible with X11, Wayland, and Cocoa IpcHelperWindow structs).
 */
struct IpcHelperWindow {
    HonkordGL_GW_Handle display;
    HonkordGL_GW_Handle window;
    unsigned long message_atom;
};

/**
 * Portable window backend: Initialize() tries each built-in driver in order until one connects.
 * On Linux: X11, then Wayland, then DRM/KMS if `HONKORDGL_ENABLE_DRM` (unless `HONKORDGL_PLATFORM_RASPBERRY_PI`, which uses only the Raspberry Pi X11 driver). On NetBSD / FreeBSD: X11 (reported as `WindowBackendKind::NetBSD` or `WindowBackendKind::FreeBSD`). On Raspberry Pi (with `HONKORDGL_PLATFORM_RASPBERRY_PI`): `WindowBackendKind::RaspberryPi`. On macOS: Cocoa. On Windows: Win32. On Android: native app glue. OpenDisplay() delegates to Initialize() when idle.
 */
class HONKORDGL_API WindowBackend {
public:
    WindowBackend() noexcept;
    ~WindowBackend();
    WindowBackend(WindowBackend&& other) noexcept;
    WindowBackend& operator=(WindowBackend&& other) noexcept;
    WindowBackend(const WindowBackend&) = delete;
    WindowBackend& operator=(const WindowBackend&) = delete;

    /**
     * Try built-in drivers in order until one succeeds (display / application connection).
     * No-op success if already initialized.
     */
    HONKORDGL_ND bool Initialize(const char * display_or_app_name = nullptr) noexcept;
    /** Close connection and clear driver selection (may call Initialize again). */
    void Shutdown() noexcept;

    HONKORDGL_ND WindowBackendKind GetKind() const noexcept;

    /** If idle, same as Initialize; otherwise forwards to the active driver. */
    HONKORDGL_ND bool OpenDisplay(const char * display_or_app_name = nullptr) noexcept;
#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    HONKORDGL_ND bool BorrowFromSession(X11::Internal::PlatformSession& session) noexcept;
    HONKORDGL_ND bool BorrowFromSession(Wayland::Internal::PlatformSession& session) noexcept;
#elif HONKORDGL_PLATFORM_RASPBERRY
    HONKORDGL_ND bool BorrowFromSession(RaspberryPi::Internal::PlatformSession& session) noexcept;
#elif HONKORDGL_PLATFORM_NETBSD
    HONKORDGL_ND bool BorrowFromSession(NetBSD::Internal::PlatformSession& session) noexcept;
#elif HONKORDGL_PLATFORM_FREEBSD
    HONKORDGL_ND bool BorrowFromSession(FreeBSD::Internal::PlatformSession& session) noexcept;
#elif HONKORDGL_PLATFORM_APPLE
    HONKORDGL_ND bool BorrowFromSession(Cocoa::Internal::PlatformSession& session) noexcept;
#elif HONKORDGL_PLATFORM_ANDROID
    /** Bind `android_app` and select the Android driver (alternative to BindNativeApp + Initialize). */
    HONKORDGL_ND bool BorrowFromAndroid(struct android_app * app) noexcept;
#endif
    void CloseDisplay() noexcept;

    /**
     * Uses ctx.window_title and ctx.client_rect (x, y, width in w, height in z).
     * On success, enables monitor hotplug / layout polling where supported (e.g. RandR on X11, output events on Wayland, Cocoa/Win32 equivalents).
     */
    HONKORDGL_ND bool CreateWindow(ApplicationContextSettings& ctx) noexcept;

    void DestroyWindow(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND bool ProcessEvents(ApplicationContextSettings& ctx) noexcept;

    void SetWindowTitle(ApplicationContextSettings& ctx, const char * title) noexcept;

    /** Update title and client_rect on an existing window without destroy/recreate. */
    void ApplyWindowSettings(ApplicationContextSettings& ctx) noexcept;
    /**
     * Overwrites `ctx` with `settings` while preserving the currently attached native handles
     * (`window_handle`, device/surface/display bindings, renderer-private fields), then applies the
     * new settings to the live window without closing it.
     */
    void ResetApplicationSettings(ApplicationContextSettings& ctx, const ApplicationContextSettings& settings) noexcept;

    void AttachDisplayToContext(ApplicationContextSettings& ctx) const noexcept;

    HONKORDGL_ND bool CreateIpcHelper(IpcHelperWindow * out, const char * message_atom_name) const noexcept;

    /** Native display handle: Display * (X11), wl_display * (Wayland), NSApplication * (Cocoa), HINSTANCE (Win32), android_app * (Android), or internal DRM tag (`int fd` first field) as void *. */
    HONKORDGL_ND void * GetDisplay() const noexcept;
    HONKORDGL_ND int GetDefaultScreen() const noexcept;

    HONKORDGL_ND bool HasXkbExtension() const noexcept;
    HONKORDGL_ND bool HasXRandrExtension() const noexcept;
    HONKORDGL_ND bool HasXInput2Extension() const noexcept;
    HONKORDGL_ND int XInput2MajorVersion() const noexcept;
    HONKORDGL_ND int XInput2MinorVersion() const noexcept;
    HONKORDGL_ND int XInputOpcode() const noexcept;

    HONKORDGL_ND int RandrEventBase() const noexcept;

    HONKORDGL_ND bool EnableRandrMonitorPolling() noexcept;
    HONKORDGL_ND bool PollMonitorsChanged() noexcept;

    /**
     * Present a CPU RGBA8 framebuffer to the active window backend.
     * Returns true when presented by the current backend implementation.
     */
    HONKORDGL_ND bool PresentSoftwareFramebuffer(
        const ApplicationContextSettings& ctx,
        const std::uint8_t * rgba_pixels,
        int width,
        int height,
        int stride_bytes) noexcept;

    /**
     * Sets the target frame rate for DelayFrame(): when > 0, DelayFrame() sleeps to pace the loop.
     * Non-positive values disable frame pacing until set again.
     */
    void SetTargetFrameRate(int fps) noexcept;

    /**
     * Sleeps for the remainder of the frame budget since the last successful DelayFrame() call
     * (or since SetTargetFrameRate reset the clock). No-op when the target rate is unset or non-positive.
     */
    void DelayFrame() noexcept;

    /**
     * Frame tick counter: incremented once each time DelayFrame() finishes while frame pacing is active
     * (SetTargetFrameRate with a positive FPS). Not wall-clock time.
     */
    HONKORDGL_ND std::uint64_t GetTicks() const noexcept;
    /** Sets the frame tick counter (see GetTicks). */
    void SetTicks(std::uint64_t ticks) noexcept;

    /** Applies a built-in cursor for the window (theme cursors on Wayland; X11 / Win32 / Cocoa; no-op on some targets). */
    void SetCursorKind(ApplicationContextSettings& ctx, CursorKind kind) noexcept;
    /**
     * ARGB8 cursor from memory (see `CursorImageParams`). Returns false only if parameters are invalid.
     * If the platform cannot apply a custom bitmap, the implementation falls back to the default arrow cursor and returns true.
     */
    HONKORDGL_ND bool SetCursorFromPixels(ApplicationContextSettings& ctx, const CursorImageParams& params) noexcept;
    /** Loads an image via stb_image (PNG, JPEG, …), then applies as cursor. Hotspot / display size come from `params`. */
    HONKORDGL_ND bool SetCursorFromImageFile(ApplicationContextSettings& ctx, const char * path, const CursorImageParams& params) noexcept;
    /** Restores the default arrow cursor. */
    void ResetCursor(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND int GetMonitorCount() const noexcept;
    HONKORDGL_ND bool GetMonitorBounds(int index, Recti * out) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace HonkordGL::Graphics

#endif