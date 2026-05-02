/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — X11 cursor helpers (font + Xcursor RGBA)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_X11_CURSORX11_HPP
#define HONKORDGL_X11_CURSORX11_HPP

#include <HonkordGL/Cursor.h>

#include <X11/Xlib.h>
#include <unordered_map>

namespace HonkordGL::Graphics::X11::Internal {

/**
 * Tracks per-window X11 cursors created by the backend; frees them on replace, window destroy, or display close.
 */
class CursorX11State {
public:
    CursorX11State() noexcept = default;
    CursorX11State(CursorX11State&&) noexcept = default;
    CursorX11State& operator=(CursorX11State&&) noexcept = default;
    CursorX11State(const CursorX11State&) = delete;
    CursorX11State& operator=(const CursorX11State&) = delete;

    void destroyAll(Display * dpy) noexcept;
    void onDestroyWindow(Display * dpy, ::Window win) noexcept;

    void setKind(Display * dpy, ::Window win, CursorKind kind) noexcept;
    bool setFromPixels(Display * dpy, ::Window win, const CursorImageParams& params) noexcept;
    void reset(Display * dpy, ::Window win) noexcept;

private:
    void replaceCursor(Display * dpy, ::Window win, ::Cursor new_cursor) noexcept;

    std::unordered_map<::Window, ::Cursor> window_cursors_;
};

} // namespace HonkordGL::Graphics::X11::Internal

#endif