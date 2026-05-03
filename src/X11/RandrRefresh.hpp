/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — RandR mode timing → refresh (Hz)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_X11_RANDRREFRESH_HPP
#define HONKORDGL_X11_RANDRREFRESH_HPP

#include <X11/extensions/Xrandr.h>

namespace HonkordGL::Graphics::X11::Internal {

/**
 * Vertical refresh rate in Hz from XRRModeInfo timing.
 *
 * RandR / X.Org convention: `dotClock` is the pixel clock in **kilohertz (kHz)**.
 * One full frame contains `hTotal * vTotal` pixel clocks (including blanking), so:
 *
 *     Hz = (dotClock * 1000) / (hTotal * vTotal)
 *
 * Interlaced modes (`modeFlags & RR_Interlace`): this value is usually the **field** rate
 * (what tools like xrandr show as “60Hz” for 1080i). For **full-frame** rate, multiply by 0.5.
 * Double-scan modes are already reflected in the timing parameters; no extra factor here.
 *
 * @return Refresh in Hz, or 0 if `hTotal` or `vTotal` is zero.
 */
inline double RandRModeRefreshHz(const XRRModeInfo& mode) noexcept
{
    const double h = static_cast<double>(mode.hTotal);
    const double v = static_cast<double>(mode.vTotal);
    if (h <= 0.0 || v <= 0.0)
        return 0.0;
    return (static_cast<double>(mode.dotClock) * 1000.0) / (h * v);
}

/** Full-frame Hz for interlaced modes; same as RandRModeRefreshHz for progressive timings. */
inline double RandRModeRefreshHzFullFrames(const XRRModeInfo& mode) noexcept
{
    double hz = RandRModeRefreshHz(mode);
    if ((mode.modeFlags & RR_Interlace) && hz > 0.0)
        hz *= 0.5;
    return hz;
}

} // namespace HonkordGL::Graphics::X11::Internal

#endif