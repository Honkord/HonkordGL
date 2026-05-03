/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — Quartz connected-display set polling (Cocoa)
    * Copyright (C) 2026 Honkord
    */

#include "DisplayMonitorPoll.hpp"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace HonkordGL::Graphics::Cocoa::Internal {

namespace {

std::mutex g_mu;
bool g_has_baseline{false};
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
/** Fingerprint of the set of active (connected) Core Graphics displays, sorted by id. */
uint64_t ActiveDisplaysFingerprint() noexcept
{
    uint32_t count = 0;
    if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess)
        return 0;
    if (count == 0)
        return Fnv1a64(nullptr, 0);

    std::vector<CGDirectDisplayID> ids(static_cast<std::size_t>(count));
    if (CGGetActiveDisplayList(count, ids.data(), &count) != kCGErrorSuccess)
        return 0;
    ids.resize(static_cast<std::size_t>(count));
    std::sort(ids.begin(), ids.end());
    return Fnv1a64(reinterpret_cast<const uint8_t *>(ids.data()), ids.size() * sizeof(CGDirectDisplayID));
}

} // namespace

bool CocoaEnableMonitorPolling() noexcept
{
    @autoreleasepool {
        const uint64_t fp = ActiveDisplaysFingerprint();
        const std::lock_guard<std::mutex> lock(g_mu);
        g_fingerprint = fp;
        g_has_baseline = true;
        return true;
    }
}
bool CocoaPollMonitorTopologyChanged() noexcept
{
    const uint64_t now = ActiveDisplaysFingerprint();
    const std::lock_guard<std::mutex> lock(g_mu);
    if (!g_has_baseline) {
        g_fingerprint = now;
        g_has_baseline = true;
        return false;
    }
    const bool changed = (now != g_fingerprint);
    if (changed)
        g_fingerprint = now;
    return changed;
}
void CocoaDisableMonitorPolling() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mu);
    g_has_baseline = false;
}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#else

namespace HonkordGL::Graphics::Cocoa::Internal {

bool CocoaEnableMonitorPolling() noexcept
{
    return false;
}
bool CocoaPollMonitorTopologyChanged() noexcept
{
    return false;
}
void CocoaDisableMonitorPolling() noexcept {}

} // namespace HonkordGL::Graphics::Cocoa::Internal

#endif