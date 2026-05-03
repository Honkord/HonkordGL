/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Cocoa monitor bounds (NSScreen)
    * Copyright (C) 2026 Honkord
    *
    * Link: -framework Cocoa
    */

#include "MonitorsCocoa.hpp"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace HonkordGL::Graphics::Cocoa::Internal {

namespace {

bool CollectMonitorRects(std::vector<Recti> & out_rects) noexcept
{
    out_rects.clear();
    @autoreleasepool {
        NSArray * const screens = [NSScreen screens];
        if (!screens || [screens count] == 0)
            return false;
        CGFloat maxY = 0.0;
        for (NSUInteger i = 0; i < [screens count]; ++i) {
            NSScreen * const s = [screens objectAtIndex:i];
            if (!s)
                continue;
            maxY = std::max(maxY, NSMaxY([s frame]));
        }
        for (NSUInteger i = 0; i < [screens count]; ++i) {
            NSScreen * const s = [screens objectAtIndex:i];
            if (!s)
                continue;
            const NSRect f = [s frame];
            Recti r{};
            r.x = static_cast<int>(std::lround(NSMinX(f)));
            r.y = static_cast<int>(std::lround(maxY - NSMaxY(f)));
            r.w = static_cast<int>(std::lround(NSWidth(f)));
            r.z = static_cast<int>(std::lround(NSHeight(f)));
            out_rects.push_back(r);
        }
        return !out_rects.empty();
    }
}

} // namespace

int GetMonitorCount() noexcept
{
    std::vector<Recti> rects;
    if (!CollectMonitorRects(rects))
        return 0;
    return static_cast<int>(rects.size());
}
bool GetMonitorBounds(int index, Recti * out) noexcept
{
    if (!out)
        return false;
    std::vector<Recti> rects;
    if (!CollectMonitorRects(rects))
        return false;
    if (index < 0 || index >= static_cast<int>(rects.size()))
        return false;
    *out = rects[static_cast<std::size_t>(index)];
    return true;
}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#else

namespace HonkordGL::Graphics::Cocoa::Internal {

int GetMonitorCount() noexcept
{
    return 0;
}
bool GetMonitorBounds(int, Recti *) noexcept
{
    return false;
}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif