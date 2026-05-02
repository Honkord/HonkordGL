/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Wayland event dequeue + non-blocking dispatch
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#if defined(__linux__) && !defined(__ANDROID__)

#include "WaylandWindowContext.hpp"

#include <wayland-client.h>

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace HonkordGL::Events
{

bool PollEventWayland(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    if (static_cast<HonkordGL::Graphics::NativePlatform>(ctx.native_platform)
        != HonkordGL::Graphics::NativePlatform::Wayland)
        return false;

    auto* wctx = reinterpret_cast<HonkordGL::Graphics::Wayland::Internal::WaylandWindowContext*>(ctx.window_handle);
    auto* dpy = reinterpret_cast<wl_display*>(ctx.device_context);
    if (!wctx || !dpy)
        return false;

    auto apply_to_ctx = [&ctx](Event& e) {
        if (e.type == EventType::RESIZE) {
            ctx.needs_resize = 1;
            ctx.client_rect.w = e.width;
            ctx.client_rect.z = e.height;
            ctx.frame_buffer_width = e.width;
            ctx.frame_buffer_height = e.height;
            if (ctx.ResizeCallback)
                ctx.ResizeCallback(&ctx, e.width, e.height);
        }
    };

    if (!wctx->pending.empty()) {
        out = wctx->pending.front();
        wctx->pending.pop_front();
        apply_to_ctx(out);
        return true;
    }

    wl_display_flush(dpy);

    while (wl_display_prepare_read(dpy) != 0) {
        if (wl_display_dispatch_pending(dpy) < 0)
            return false;
    }

    const int fd = wl_display_get_fd(dpy);
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 0) <= 0) {
        wl_display_cancel_read(dpy);
        return false;
    }

    if (wl_display_read_events(dpy) < 0) {
        wl_display_cancel_read(dpy);
        return false;
    }
    wl_display_dispatch_pending(dpy);

    if (!wctx->pending.empty()) {
        out = wctx->pending.front();
        wctx->pending.pop_front();
        apply_to_ctx(out);
        return true;
    }

    return false;
}

} // namespace HonkordGL::Events

#endif
