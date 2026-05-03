/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — internal Cocoa + GLFW bootstrap (library implementation detail)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_PLATFORMSESSION_HPP
#define HONKORDGL_COCOA_PLATFORMSESSION_HPP

#include <HonkordGL/Config.h>

namespace HonkordGL::Graphics::Cocoa::Internal {

/**
 * NSApplication bootstrap and optional GLFW init.
 *
 * Typical sequence:
 *  1. connect_display  — shared NSApplication + activation policy
 *  2. setup_core_cocoa — reserved (notifications / workspace hooks)
 *  3. prepare_glfw     — glfwInit (requires -DHONKORDGL_USE_GLFW)
 *
 * Or call bootstrap() for all three. GLFW uses its own Cocoa backend; this session is still
 * used for HonkordGL::Graphics::Cocoa::WindowBackend via BorrowFromSession.
 *
 * Shutdown: release WindowBackend borrow, then shutdown() (glfwTerminate only).
 */
class PlatformSession {
public:
    PlatformSession() noexcept = default;
    ~PlatformSession();

    PlatformSession(const PlatformSession&) = delete;
    PlatformSession& operator=(const PlatformSession&) = delete;
    PlatformSession(PlatformSession&& other) noexcept;
    PlatformSession& operator=(PlatformSession&& other) noexcept;

    /** Step 1: NSApplication + activation policy. */
    HONKORDGL_ND bool connect_display() noexcept;

    /** Step 2: optional core hooks (no-op today). */
    void setup_core_cocoa() noexcept;

    /** Step 3: GLFW init for Cocoa (macro HONKORDGL_USE_GLFW). */
    HONKORDGL_ND bool prepare_glfw() noexcept;

    HONKORDGL_ND bool bootstrap() noexcept;

    void shutdown() noexcept;

    HONKORDGL_ND bool is_connected() const noexcept { return app_ready_; }
    /** NSApplication * as void *. */
    HONKORDGL_ND void * application() const noexcept;
    HONKORDGL_ND int default_screen() const noexcept { return screen_; }
    HONKORDGL_ND bool glfw_initialized() const noexcept { return glfw_ok_; }

private:
    void terminate_glfw() noexcept;

    bool app_ready_{false};
    int screen_{0};
    bool glfw_ok_{false};
};

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif