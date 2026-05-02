/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — X11 cursor helpers (implementation)
 * Copyright (C) 2026 Honkord
 */

#include "CursorX11.hpp"

#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace HonkordGL::Graphics::X11::Internal {

namespace {

unsigned int shapeForCursorKind(CursorKind k) noexcept
{
    switch (k) {
    case CursorKind::Arrow:
        return XC_left_ptr;
    case CursorKind::ArrowWait:
    case CursorKind::Wait:
        return XC_watch;
    case CursorKind::Text:
        return XC_xterm;
    case CursorKind::Hand:
        return XC_hand2;
    case CursorKind::SizeHorizontal:
        return XC_sb_h_double_arrow;
    case CursorKind::SizeVertical:
        return XC_sb_v_double_arrow;
    case CursorKind::SizeTopLeftBottomRight:
        return XC_ul_angle;
    case CursorKind::SizeBottomLeftTopRight:
        return XC_lr_angle;
#if defined(__linux__)
    case CursorKind::SizeLeft:
        return XC_sb_left_arrow;
    case CursorKind::SizeRight:
        return XC_sb_right_arrow;
    case CursorKind::SizeTop:
        return XC_sb_up_arrow;
    case CursorKind::SizeBottom:
        return XC_sb_down_arrow;
    case CursorKind::SizeTopLeft:
        return XC_top_left_corner;
    case CursorKind::SizeBottomRight:
        return XC_bottom_right_corner;
    case CursorKind::SizeBottomLeft:
        return XC_bottom_left_corner;
    case CursorKind::SizeTopRight:
        return XC_top_right_corner;
#else
    case CursorKind::SizeLeft:
    case CursorKind::SizeRight:
        return XC_sb_h_double_arrow;
    case CursorKind::SizeTop:
    case CursorKind::SizeBottom:
        return XC_sb_v_double_arrow;
    case CursorKind::SizeTopLeft:
    case CursorKind::SizeBottomRight:
        return XC_ul_angle;
    case CursorKind::SizeBottomLeft:
    case CursorKind::SizeTopRight:
        return XC_lr_angle;
#endif
    case CursorKind::SizeAll:
        return XC_fleur;
    case CursorKind::Cross:
        return XC_crosshair;
    case CursorKind::Help:
        return XC_question_arrow;
    case CursorKind::NotAllowed:
        return XC_circle;
    default:
        return XC_left_ptr;
    }
}

void resizeNearestRgba(
    const std::uint8_t * src,
    int sw,
    int sh,
    int sstride,
    int dw,
    int dh,
    std::vector<std::uint8_t> * out) noexcept
{
    out->resize(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh) * 4u);
    for (int y = 0; y < dh; ++y) {
        const int sy = (y * sh) / dh;
        for (int x = 0; x < dw; ++x) {
            const int sx = (x * sw) / dw;
            const std::uint8_t * const p = src + static_cast<std::size_t>(sy) * static_cast<std::size_t>(sstride)
                + static_cast<std::size_t>(sx) * 4u;
            std::uint8_t * const d = out->data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(dw) + static_cast<std::size_t>(x)) * 4u;
            std::memcpy(d, p, 4u);
        }
    }
}

} // namespace

void CursorX11State::destroyAll(Display * dpy) noexcept
{
    if (!dpy) {
        window_cursors_.clear();
        return;
    }
    for (const auto& e : window_cursors_) {
        if (e.second != None)
            XFreeCursor(dpy, e.second);
    }
    window_cursors_.clear();
}

void CursorX11State::onDestroyWindow(Display * dpy, ::Window win) noexcept
{
    const auto it = window_cursors_.find(win);
    if (it == window_cursors_.end())
        return;
    if (dpy && it->second != None)
        XFreeCursor(dpy, it->second);
    window_cursors_.erase(it);
}

void CursorX11State::replaceCursor(Display * dpy, ::Window win, ::Cursor new_cursor) noexcept
{
    if (!dpy || !win || new_cursor == None)
        return;
    XDefineCursor(dpy, win, new_cursor);
    const auto it = window_cursors_.find(win);
    if (it != window_cursors_.end() && it->second != None)
        XFreeCursor(dpy, it->second);
    window_cursors_[win] = new_cursor;
}

void CursorX11State::setKind(Display * dpy, ::Window win, CursorKind kind) noexcept
{
    if (!dpy || !win)
        return;
    const unsigned int shape = shapeForCursorKind(kind);
    const ::Cursor c = XCreateFontCursor(dpy, shape);
    if (c == None)
        return;
    replaceCursor(dpy, win, c);
}

bool CursorX11State::setFromPixels(Display * dpy, ::Window win, const CursorImageParams& params) noexcept
{
    if (!dpy || !win || !params.rgba_pixels)
        return false;
    int w = params.width;
    int h = params.height;
    if (w <= 0 || h <= 0)
        return false;
    const int sstride = params.stride_bytes > 0 ? params.stride_bytes : (w * 4);
    if (sstride < w * 4)
        return false;

    int dw = params.display_width > 0 ? params.display_width : w;
    int dh = params.display_height > 0 ? params.display_height : h;
    if (dw <= 0)
        dw = w;
    if (dh <= 0)
        dh = h;

    const std::uint8_t * src = params.rgba_pixels;
    std::vector<std::uint8_t> resized;
    if (dw != w || dh != h) {
        resizeNearestRgba(params.rgba_pixels, w, h, sstride, dw, dh, &resized);
        src = resized.data();
        w = dw;
        h = dh;
    }

    int hx = params.hotspot_x;
    int hy = params.hotspot_y;
    if (dw != params.width || dh != params.height) {
        if (params.width > 0 && params.height > 0) {
            hx = (hx * dw) / params.width;
            hy = (hy * dh) / params.height;
        }
    }
    hx = std::clamp(hx, 0, w - 1);
    hy = std::clamp(hy, 0, h - 1);

    XcursorImage * const ximg = XcursorImageCreate(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    if (!ximg)
        return false;
    ximg->xhot = static_cast<unsigned int>(hx);
    ximg->yhot = static_cast<unsigned int>(hy);
    ximg->delay = 0;
    ximg->size = static_cast<unsigned int>(std::max(w, h));

    const int row_stride = w * 4;
    for (int y = 0; y < h; ++y) {
        const std::uint8_t * row = src + static_cast<std::size_t>(y) * static_cast<std::size_t>(row_stride);
        for (int x = 0; x < w; ++x) {
            const std::uint8_t * p = row + static_cast<std::size_t>(x) * 4u;
            const std::uint32_t argb = (static_cast<std::uint32_t>(p[3]) << 24u) | (static_cast<std::uint32_t>(p[0]) << 16u)
                | (static_cast<std::uint32_t>(p[1]) << 8u) | static_cast<std::uint32_t>(p[2]);
            ximg->pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = argb;
        }
    }

    XcursorImages * const imgs = XcursorImagesCreate(1);
    if (!imgs) {
        XcursorImageDestroy(ximg);
        return false;
    }
    imgs->nimage = 1;
    imgs->images[0] = ximg;
    const ::Cursor c = XcursorImagesLoadCursor(dpy, imgs);
    XcursorImagesDestroy(imgs);
    if (c == None)
        return false;
    replaceCursor(dpy, win, c);
    return true;
}

void CursorX11State::reset(Display * dpy, ::Window win) noexcept
{
    setKind(dpy, win, CursorKind::Arrow);
}

} // namespace HonkordGL::Graphics::X11::Internal