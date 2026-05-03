/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — main display refresh rate (Quartz)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_DISPLAYREFRESH_HPP
#define HONKORDGL_COCOA_DISPLAYREFRESH_HPP

#if defined(__APPLE__)

#include <CoreGraphics/CoreGraphics.h>

namespace HonkordGL::Graphics::Cocoa {

/** Returns refresh rate in Hz for the main display, or 0 if unknown. */
inline double MainDisplayRefreshRateHz() noexcept
{
    const CGDirectDisplayID main = CGMainDisplayID();
    CGDisplayModeRef const mode = CGDisplayCopyDisplayMode(main);
    if (!mode)
        return 0.0;
    const double hz = CGDisplayModeGetRefreshRate(mode);
    CFRelease(mode);
    return hz;
}

} // namespace HonkordGL::Graphics::Cocoa

#endif

#endif