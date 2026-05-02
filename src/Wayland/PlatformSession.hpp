/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland display + optional GLFW bootstrap
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_WAYLAND_PLATFORMSESSION_HPP
#define HONKORDGL_WAYLAND_PLATFORMSESSION_HPP

#include <HonkordGL/Config.h>

struct wl_display;

namespace HonkordGL::Graphics::Wayland::Internal {

/**
 * Owns a wl_display connection (optional) and optional GLFW init (HONKORDGL_USE_GLFW).
 * Typical sequence matches X11: connect_display → prepare_glfw, or bootstrap().
 */
class PlatformSession {
public:
    PlatformSession() noexcept = default;
    ~PlatformSession();

    PlatformSession(const PlatformSession&) = delete;
    PlatformSession& operator=(const PlatformSession&) = delete;
    PlatformSession(PlatformSession&& other) noexcept;
    PlatformSession& operator=(PlatformSession&& other) noexcept;

    HONKORDGL_ND bool connect_display(const char * name = nullptr) noexcept;
    void shutdown() noexcept;

    HONKORDGL_ND bool prepare_glfw() noexcept;
    HONKORDGL_ND bool bootstrap(const char * name = nullptr) noexcept;

    HONKORDGL_ND bool is_connected() const noexcept { return dpy_ != nullptr; }
    HONKORDGL_ND wl_display* display() const noexcept { return dpy_; }

private:
    void terminate_glfw() noexcept;

    wl_display * dpy_{nullptr};
    bool owns_display_{false};
    bool glfw_ok_{false};
};

} // namespace HonkordGL::Graphics::Wayland::Internal

#endif