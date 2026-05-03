/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — poll for RandR monitor / output topology changes (X11)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_RANDRMONITORPOLL_HPP
#define HONKORDGL_X11_RANDRMONITORPOLL_HPP

#include <X11/Xlib.h>

namespace HonkordGL::Graphics::X11::Internal {

/**
 * Select RandR notifications on the root window for this display connection.
 * Call once after XRRQueryExtension succeeds (e.g. from WindowBackend after OpenDisplay).
 */
bool RandrEnableMonitorPolling(Display * dpy, int screen) noexcept;

/**
 * Non-blocking: drains matching RandR events from the queue and compares the set of
 * connected RandR outputs to the last snapshot (plug, unplug, mode set, etc.).
 * @param rr_event_base from XRRQueryExtension (same as WindowBackend::RandrEventBase()).
 */
bool RandrPollMonitorTopologyChanged(Display * dpy, int screen, int rr_event_base) noexcept;

} // namespace HonkordGL::Graphics::X11::Internal

#endif