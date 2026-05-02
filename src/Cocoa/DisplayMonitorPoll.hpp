/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Quartz display layout change polling (Cocoa)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_DISPLAYMONITORPOLL_HPP
#define HONKORDGL_COCOA_DISPLAYMONITORPOLL_HPP

namespace HonkordGL::Graphics::Cocoa::Internal {

/** Baselines the current set of active Core Graphics displays (call once when enabling). */
bool CocoaEnableMonitorPolling() noexcept;
/** Non-blocking: true if the set of connected/active displays changed since last poll or enable. */
bool CocoaPollMonitorTopologyChanged() noexcept;
void CocoaDisableMonitorPolling() noexcept;

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif