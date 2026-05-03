/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — internal X11 + GLFW bootstrap (library implementation detail)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_PLATFORMSESSION_HPP
#define HONKORDGL_X11_PLATFORMSESSION_HPP

#include <HonkordGL/Config.h>

#include <X11/Xlib.h>

namespace HonkordGL::Graphics::X11::Internal {

/**
 * Owns one X display connection (optional), core ICCCM atoms, and optional GLFW init.
 *
 * Typical sequence (matches intended bring-up order):
 *  1. connect_display  — XOpenDisplay
 *  2. setup_core_x11   — WM_PROTOCOLS / WM_DELETE_WINDOW, flush
 *  3. prepare_glfw     — glfwInit (requires -DHONKORDGL_USE_GLFW and link libglfw)
 *
 * Or call bootstrap() for all three. GLFW opens its own X connection internally; this
 * session’s Display is still used for HonkordGL::Graphics::X11::WindowBackend via BorrowFromSession.
 *
 * Shutdown order: release WindowBackend borrow, then shutdown() (glfwTerminate then XCloseDisplay).
 */
class PlatformSession {
public:
    PlatformSession() noexcept = default;
    ~PlatformSession();

    PlatformSession(const PlatformSession&) = delete;
    PlatformSession& operator=(const PlatformSession&) = delete;
    PlatformSession(PlatformSession&& other) noexcept;
    PlatformSession& operator=(PlatformSession&& other) noexcept;

    /** Step 1: XOpenDisplay. Sets owns_display. */
    HONKORDGL_ND bool connect_display(const char * display_name = nullptr) noexcept;

    /** Step 2: intern WM_PROTOCOLS / WM_DELETE_WINDOW, XFlush. No-op if not connected. */
    void setup_core_x11() noexcept;

    /** Step 3: GLFW init for X11 (macro HONKORDGL_USE_GLFW). No-op success when macro unset. */
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

} // namespace HonkordGL::Graphics::X11::Internal

#endif