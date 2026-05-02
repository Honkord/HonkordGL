/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — DRM/KMS event dequeue (PollEvent)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#if defined(__linux__) && !defined(__ANDROID__)

#if defined(HONKORDGL_ENABLE_DRM)
#include "DRMWindowContext.hpp"
#endif

namespace HonkordGL::Events {

#if defined(HONKORDGL_ENABLE_DRM)

bool PollEventDrm(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    if (static_cast<Graphics::NativePlatform>(ctx.native_platform) != Graphics::NativePlatform::DRM)
        return false;
    auto * const st = reinterpret_cast<Graphics::DRM::Internal::DrmWindowState *>(ctx.window_handle);
    if (!st || st->pending.empty())
        return false;
    out = st->pending.front();
    st->pending.pop_front();
    return true;
}

#else

bool PollEventDrm(Graphics::ApplicationContextSettings&, Event&) noexcept
{
    return false;
}

#endif

} // namespace HonkordGL::Events

#endif