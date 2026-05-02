/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Win32 event dequeue (PollEvent)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#include "Win32WindowContext.hpp"

#if defined(_WIN32)

namespace HonkordGL::Events {

bool PollEvent(Graphics::ApplicationContextSettings& ctx, Event& out) noexcept
{
    if (static_cast<Graphics::NativePlatform>(ctx.native_platform) != Graphics::NativePlatform::Win32)
        return false;
    auto * const st = reinterpret_cast<Graphics::Win32::Internal::Win32WindowState *>(ctx.window_handle);
    if (!st)
        return false;

    // Keep Win32 responsive even when callers only use PollEvent (most works/* loops).
    while (st->pending.empty()) {
        MSG msg{};
        if (!PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            break;
        if (msg.message == WM_QUIT) {
            Event e{};
            e.type = EventType::QUIT;
            e.key = KeyCode::NULL_KEY;
            e.mouse_button = 0;
            e.x = 0;
            e.y = 0;
            e.width = 0;
            e.height = 0;
            e.focused = 0;
            e.modifiers = 0;
            e.character = 0;
            st->pending.push_back(e);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (st->pending.empty())
        return false;

    out = st->pending.front();
    st->pending.pop_front();
    return true;
}

} // namespace HonkordGL::Events

#endif
