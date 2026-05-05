/**
 * HonkordGL — X11 / Wayland native connection helpers (Linux desktop)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/LinuxDisplayIntegration.h>
#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/WindowApplication.h>

#if HONKORDGL_PLATFORM_LINUX_DESKTOP

#include <X11/Xlib.h>

#include "Wayland/WaylandWindowContext.hpp"

namespace HonkordGL::Graphics {

unsigned long LinuxDisplayIntegrationX11InternAtom(
    HonkordGL_GW_Handle display,
    char const * atom_name,
    int only_if_exists) noexcept
{
    if (!display || !atom_name)
        return 0;
    Display * const dpy = reinterpret_cast<Display *>(display);
    return static_cast<unsigned long>(XInternAtom(dpy, atom_name, only_if_exists ? True : False));
}

unsigned long LinuxDisplayIntegrationX11DefaultRootWindow(HonkordGL_GW_Handle display) noexcept
{
    if (!display)
        return 0;
    Display * const dpy = reinterpret_cast<Display *>(display);
    return static_cast<unsigned long>(DefaultRootWindow(dpy));
}

HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetDisplay(HonkordGL_GW_Handle window_context) noexcept
{
    if (!window_context)
        return nullptr;
    auto * const wctx = reinterpret_cast<Wayland::Internal::WaylandWindowContext *>(window_context);
    return reinterpret_cast<HonkordGL_GW_Handle>(wctx->display);
}

HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetSurface(HonkordGL_GW_Handle window_context) noexcept
{
    if (!window_context)
        return nullptr;
    auto * const wctx = reinterpret_cast<Wayland::Internal::WaylandWindowContext *>(window_context);
    return reinterpret_cast<HonkordGL_GW_Handle>(wctx->surface);
}

} // namespace HonkordGL::Graphics

#else

namespace HonkordGL::Graphics {

unsigned long LinuxDisplayIntegrationX11InternAtom(HonkordGL_GW_Handle, char const *, int) noexcept
{
    return 0;
}

unsigned long LinuxDisplayIntegrationX11DefaultRootWindow(HonkordGL_GW_Handle) noexcept
{
    return 0;
}

HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetDisplay(HonkordGL_GW_Handle) noexcept
{
    return nullptr;
}

HonkordGL_GW_Handle LinuxDisplayIntegrationWaylandGetSurface(HonkordGL_GW_Handle) noexcept
{
    return nullptr;
}

} // namespace HonkordGL::Graphics

#endif
