/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Android event dequeue (PollEvent)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#include "AndroidWindowContext.hpp"

#if defined(__ANDROID__)

namespace HonkordGL::Events {

bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    if (static_cast<Graphics::NativePlatform>(ctx.native_platform) != Graphics::NativePlatform::Android)
        return false;
    auto * const st = reinterpret_cast<Graphics::Android::Internal::AndroidWindowState *>(ctx.window_handle);
    if (!st || st->pending.empty())
        return false;
    out = st->pending.front();
    st->pending.pop_front();
    return true;
}

} // namespace HonkordGL::Events

#endif // __ANDROID__
