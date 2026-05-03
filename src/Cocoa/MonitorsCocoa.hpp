/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa monitor bounds (NSScreen)
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_COCOA_MONITORS_HPP
#define HONKORDGL_COCOA_MONITORS_HPP

#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics::Cocoa::Internal {

int GetMonitorCount() noexcept;
bool GetMonitorBounds(int index, Recti * out) noexcept;

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif
