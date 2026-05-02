/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — per-window Win32 state (implementation detail)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_WIN32_WINDOWCONTEXT_HPP
#define HONKORDGL_WIN32_WINDOWCONTEXT_HPP

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>

#include <deque>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#ifdef DELETE
#undef DELETE
#endif
#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

namespace HonkordGL::Graphics::Win32::Internal {

struct Win32WindowState {
    HWND hwnd{nullptr};
    ApplicationContextSettings * ctx{nullptr};
    std::deque<HonkordGL::Events::Event> pending;
    bool track_leave{false};
    HCURSOR user_cursor{nullptr};
    bool user_cursor_owned{false};
};

} // namespace HonkordGL::Graphics::Win32::Internal

#endif