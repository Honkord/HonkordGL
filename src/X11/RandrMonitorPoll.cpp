/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — RandR monitor topology polling
    * Copyright (C) 2026 Honkord
    *
    * Link: -lX11 -lXrandr
    */

#include "RandrMonitorPoll.hpp"

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace HonkordGL::Graphics::X11::Internal
{

namespace
{

struct RrPollCtx {
    int rr_base;
    Window root;
};

extern "C" Bool HonkordRrTopologyPredicate(Display*, XEvent* ev, XPointer arg) noexcept
{
    const auto* ctx = reinterpret_cast<const RrPollCtx*>(arg);
    const int t = ev->type;
    const int base = ctx->rr_base;
    const Window root = ctx->root;

    if (t == base + RRScreenChangeNotify)
        return (ev->xany.window == root) ? True : False;

    if (t == base + RRNotify) {
        const auto* n = reinterpret_cast<const XRRNotifyEvent*>(ev);
        if (n->subtype == RRNotify_OutputChange || n->subtype == RRNotify_CrtcChange)
            return True;
    }

    return False;
}

std::mutex g_mu;
Display * g_dpy{nullptr};
int g_screen{-1};
uint64_t g_fingerprint{0};

uint64_t Fnv1a64(const uint8_t * data, std::size_t len) noexcept
{
    uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}
/**
 * Fingerprint of the set of connected RandR outputs (sorted RROutput ids).
 * Stable across reordering of enumeration order.
 */
uint64_t ConnectedOutputsFingerprint(Display * dpy, int screen) noexcept
{
    if (!dpy || screen < 0 || screen >= ScreenCount(dpy))
        return 0;

    const Window root = RootWindow(dpy, screen);
    XRRScreenResources * const res = XRRGetScreenResources(dpy, root);
    if (!res)
        return 0;

    std::vector<RROutput> connected;
    connected.reserve(static_cast<std::size_t>(res->noutput));
    for (int i = 0; i < res->noutput; ++i) {
        XRROutputInfo * const oi = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (!oi)
            continue;
        if (oi->connection == RR_Connected)
            connected.push_back(res->outputs[i]);
        XRRFreeOutputInfo(oi);
    }
    XRRFreeScreenResources(res);

    std::sort(connected.begin(), connected.end());
    if (connected.empty())
        return Fnv1a64(nullptr, 0);

    return Fnv1a64(reinterpret_cast<const uint8_t *>(connected.data()), connected.size() * sizeof(RROutput));
}

} // namespace

bool RandrEnableMonitorPolling(Display * dpy, int screen) noexcept
{
    if (!dpy || screen < 0 || screen >= ScreenCount(dpy))
        return false;

    int ev_base = 0;
    int err_base = 0;
    if (!XRRQueryExtension(dpy, &ev_base, &err_base))
        return false;

    const Window root = RootWindow(dpy, screen);
    XRRSelectInput(
        dpy,
        root,
        RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask | RROutputChangeNotifyMask);
    XFlush(dpy);

    const uint64_t fp = ConnectedOutputsFingerprint(dpy, screen);
    const std::lock_guard<std::mutex> lock(g_mu);
    g_dpy = dpy;
    g_screen = screen;
    g_fingerprint = fp;
    return true;
}
bool RandrPollMonitorTopologyChanged(Display * dpy, int screen, int rr_event_base) noexcept
{
    if (!dpy || screen < 0 || screen >= ScreenCount(dpy))
        return false;

    const Window root = RootWindow(dpy, screen);
    RrPollCtx ctx{ rr_event_base, root };
    bool ev_changed = false;
    XEvent ev{};
    while (XCheckIfEvent(dpy, &ev, HonkordRrTopologyPredicate, reinterpret_cast<XPointer>(&ctx)) != 0)
        ev_changed = true;

    const uint64_t now = ConnectedOutputsFingerprint(dpy, screen);

    const std::lock_guard<std::mutex> lock(g_mu);
    if (g_dpy != dpy || g_screen != screen) {
        g_dpy = dpy;
        g_screen = screen;
        g_fingerprint = now;
        return ev_changed;
    }

    const bool fp_changed = (now != g_fingerprint);
    if (fp_changed)
        g_fingerprint = now;
    return ev_changed || fp_changed;
}

} // namespace HonkordGL::Graphics::X11::Internal

#else

namespace HonkordGL::Graphics::X11::Internal
{

bool RandrEnableMonitorPolling(Display *, int) noexcept
{
    return false;
}
bool RandrPollMonitorTopologyChanged(Display *, int, int) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::X11::Internal

#endif