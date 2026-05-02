/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — X11 monitor bounds (RandR)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_MONITORS_HPP
#define HONKORDGL_X11_MONITORS_HPP

#include <HonkordGL/WindowApplication.h>

#include <X11/Xlib.h>

namespace HonkordGL::Graphics::X11::Internal {

int GetMonitorCount(Display * dpy, int screen) noexcept;
bool GetMonitorBounds(Display * dpy, int screen, int index, Recti * out) noexcept;

} // namespace HonkordGL::Graphics::X11::Internal

#endif