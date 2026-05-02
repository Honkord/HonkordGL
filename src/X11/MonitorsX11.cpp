/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 monitor bounds (RandR)
    * Copyright (C) 2026 Honkord
    *
    * Link: -lX11 -lXrandr
    */

#include "MonitorsX11.hpp"

#if defined(HONKORDGL_PLATFORM_USES_X11_SOURCES)

#include <X11/extensions/Xrandr.h>

#include <vector>

namespace HonkordGL::Graphics::X11::Internal {

namespace {

bool CollectMonitorRects(Display * dpy, int screen, std::vector<Recti> & out_rects) noexcept
{
    out_rects.clear();
    if (!dpy || screen < 0 || screen >= ScreenCount(dpy))
        return false;

    const Window root = RootWindow(dpy, screen);

    int maj = 0;
    int min = 0;
    if (XRRQueryVersion(dpy, &maj, &min) && (maj > 1 || (maj == 1 && min >= 5))) {
        int nmon = 0;
        XRRMonitorInfo * const mon = XRRGetMonitors(dpy, root, True, &nmon);
        if (mon) {
            if (nmon > 0) {
                for (int i = 0; i < nmon; ++i) {
                    Recti r{};
                    r.x = mon[i].x;
                    r.y = mon[i].y;
                    r.w = mon[i].width;
                    r.z = mon[i].height;
                    out_rects.push_back(r);
                }
                XRRFreeMonitors(mon);
                return true;
            }
            XRRFreeMonitors(mon);
        }
    }

    XRRScreenResources * const res = XRRGetScreenResources(dpy, root);
    if (!res) {
        Recti r{};
        r.x = 0;
        r.y = 0;
        r.w = DisplayWidth(dpy, screen);
        r.z = DisplayHeight(dpy, screen);
        out_rects.push_back(r);
        return true;
    }

    for (int c = 0; c < res->ncrtc; ++c) {
        XRRCrtcInfo * const ci = XRRGetCrtcInfo(dpy, res, res->crtcs[c]);
        if (!ci)
            continue;
        if (ci->mode == None) {
            XRRFreeCrtcInfo(ci);
            continue;
        }
        Recti r{};
        r.x = ci->x;
        r.y = ci->y;
        r.w = static_cast<int>(ci->width);
        r.z = static_cast<int>(ci->height);
        out_rects.push_back(r);
        XRRFreeCrtcInfo(ci);
    }
    XRRFreeScreenResources(res);

    if (out_rects.empty()) {
        Recti r{};
        r.x = 0;
        r.y = 0;
        r.w = DisplayWidth(dpy, screen);
        r.z = DisplayHeight(dpy, screen);
        out_rects.push_back(r);
    }
    return true;
}

} // namespace

int GetMonitorCount(Display * dpy, int screen) noexcept
{
    std::vector<Recti> rects;
    if (!CollectMonitorRects(dpy, screen, rects))
        return 0;
    return static_cast<int>(rects.size());
}
bool GetMonitorBounds(Display * dpy, int screen, int index, Recti * out) noexcept
{
    if (!out)
        return false;
    std::vector<Recti> rects;
    if (!CollectMonitorRects(dpy, screen, rects))
        return false;
    if (index < 0 || index >= static_cast<int>(rects.size()))
        return false;
    *out = rects[static_cast<std::size_t>(index)];
    return true;
}

} // namespace HonkordGL::Graphics::X11::Internal

#else

namespace HonkordGL::Graphics::X11::Internal {

int GetMonitorCount(Display *, int) noexcept
{
    return 0;
}
bool GetMonitorBounds(Display *, int, int, Recti *) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::X11::Internal

#endif