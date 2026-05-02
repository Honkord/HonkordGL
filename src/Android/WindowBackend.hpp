/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Android / ANativeWindow window backend
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_ANDROID_WINDOWBACKEND_HPP
#define HONKORDGL_ANDROID_WINDOWBACKEND_HPP

#include <HonkordGL/WindowBackend.h>

#if defined(__ANDROID__)
#include <android/input.h>
#else
struct AInputEvent;
#endif

struct android_app;

namespace HonkordGL::Graphics::Android {

/**
 * Android binding for `ApplicationContextSettings` (NDK `android_native_app_glue`).
 *
 * - Call `HonkordGL::Graphics::Android::BindNativeApp(app)` from `android_main` before `WindowBackend::Initialize`.
 * - `window_handle`: internal `AndroidWindowState *`
 * - `device_context`: `android_app *`
 * - `CreateWindow` requires `app->window` (after `APP_CMD_INIT_WINDOW`).
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

    HONKORDGL_ND bool AttachApp(android_app * app) noexcept;

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

private:
    bool display_open_{false};
    android_app * app_{nullptr};
    bool hooks_installed_{false};
    void (*saved_on_app_cmd_)(android_app *, int32_t){nullptr};
    int32_t (*saved_on_input_event_)(android_app *, AInputEvent *){nullptr};
};

} // namespace HonkordGL::Graphics::Android

#endif