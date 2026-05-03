/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — per-window Wayland state (implementation detail)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_WAYLAND_WINDOWCONTEXT_HPP
#define HONKORDGL_WAYLAND_WINDOWCONTEXT_HPP

#include <HonkordGL/Cursor.h>
#include <HonkordGL/Events.h>

#include <cstddef>
#include <deque>

#include <wayland-client.h>

#include "generated/xdg-shell-client-protocol.h"

struct wl_egl_window;
struct wl_buffer;
struct wl_shm_pool;

namespace HonkordGL::Graphics::Wayland::Internal {

struct WaylandWindowContext {
    wl_display * display{nullptr};
    wl_surface * surface{nullptr};
    xdg_surface * xdg_shell_surface{nullptr};
    xdg_toplevel * xdg_shell_toplevel{nullptr};
    wl_egl_window * egl_window{nullptr};

    void * egl_display{nullptr};
    void * egl_context{nullptr};
    void * egl_surface{nullptr};

    int width{0};
    int height{0};
    bool configured{false};
    bool closed{false};

    /** Desired cursor when the pointer is over this surface (Wayland applies on enter and SetCursor*). */
    CursorKind cursor_kind{CursorKind::Arrow};
    bool cursor_is_custom{false};
    int cursor_hotspot_x{0};
    int cursor_hotspot_y{0};
    int cursor_custom_w{0};
    int cursor_custom_h{0};
    wl_buffer * cursor_custom_buffer{nullptr};
    wl_shm_pool * cursor_custom_pool{nullptr};
    void * cursor_custom_mmap{nullptr};
    std::size_t cursor_custom_mmap_size{0};
    int cursor_custom_fd{-1};

    std::deque<HonkordGL::Events::Event> pending;

    void push_event(const HonkordGL::Events::Event& e) noexcept
    {
        pending.push_back(e);
    }
};

} // namespace HonkordGL::Graphics::Wayland::Internal

#endif