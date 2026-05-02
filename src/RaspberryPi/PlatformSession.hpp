/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — internal Raspberry Pi (Linux) + X11 + optional GLFW bootstrap (library implementation detail)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_RASPBERRY_PI_PLATFORMSESSION_HPP
#define HONKORDGL_RASPBERRY_PI_PLATFORMSESSION_HPP

#include <HonkordGL/Config.h>

#include <X11/Xlib.h>

namespace HonkordGL::Graphics::RaspberryPi::Internal {

/**
 * Owns one X display connection (optional), core ICCCM atoms, and optional GLFW init.
 *
 * Same bring-up as `X11::Internal::PlatformSession`; use with `RaspberryPi::WindowBackend::BorrowFromSession`.
 *
 * Build with `-DHONKORDGL_PLATFORM_RASPBERRY_PI` on Raspberry Pi OS (or other Linux) to select this driver
 * instead of the default desktop Linux X11/Wayland driver chain.
 *
 * Typical sequence:
 *  1. connect_display  — XOpenDisplay
 *  2. setup_core_x11 — WM_PROTOCOLS / WM_DELETE_WINDOW, flush
 *  3. prepare_glfw     — glfwInit (requires -DHONKORDGL_USE_GLFW and link libglfw)
 *
 * Or call bootstrap() for all three.
 */
class PlatformSession {
public:
    PlatformSession() noexcept = default;
    ~PlatformSession();

    PlatformSession(const PlatformSession&) = delete;
    PlatformSession& operator=(const PlatformSession&) = delete;
    PlatformSession(PlatformSession&& other) noexcept;
    PlatformSession& operator=(PlatformSession&& other) noexcept;

    HONKORDGL_ND bool connect_display(const char * display_name = nullptr) noexcept;

    void setup_core_x11() noexcept;

    HONKORDGL_ND bool prepare_glfw() noexcept;

    HONKORDGL_ND bool bootstrap(const char * display_name = nullptr) noexcept;

    void shutdown() noexcept;

    HONKORDGL_ND bool is_connected() const noexcept { return dpy_ != nullptr; }
    HONKORDGL_ND Display * display() const noexcept { return dpy_; }
    HONKORDGL_ND int default_screen() const noexcept { return screen_; }
    HONKORDGL_ND Atom wm_protocols_atom() const noexcept { return wm_protocols_; }
    HONKORDGL_ND Atom wm_delete_window_atom() const noexcept { return wm_delete_; }
    HONKORDGL_ND bool glfw_initialized() const noexcept { return glfw_ok_; }

private:
    void terminate_glfw() noexcept;

    Display * dpy_{nullptr};
    int screen_{0};
    bool owns_display_{false};
    Atom wm_protocols_{None};
    Atom wm_delete_{None};
    bool glfw_ok_{false};
};

} // namespace HonkordGL::Graphics::RaspberryPi::Internal

#endif