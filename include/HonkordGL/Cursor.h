/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — window cursor kinds and custom cursor parameters
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_CURSOR_H
#define HONKORDGL_CURSOR_H

#include <HonkordGL/Config.h>

#include <cstdint>

namespace HonkordGL
{
namespace Graphics 
{

/**
 * Built-in system cursor shapes. Platform mapping may collapse variants (e.g. resize variants)
 * where the OS does not expose a distinct cursor.
 */
enum class CursorKind : std::uint16_t {
    Arrow, //!< Arrow cursor (default)
    ArrowWait, //!< Busy arrow cursor
    Wait, //!< Busy cursor
    Text, //!< I-beam over editable text
    Hand, //!< Pointing hand
    SizeHorizontal, //!< Horizontal double arrow
    SizeVertical, //!< Vertical double arrow
    SizeTopLeftBottomRight, //!< Diagonal NW–SE
    SizeBottomLeftTopRight, //!< Diagonal NE–SW
    SizeLeft, //!< Left arrow on Linux X11; elsewhere often same as SizeHorizontal
    SizeRight, //!< Right arrow on Linux X11; elsewhere often same as SizeHorizontal
    SizeTop, //!< Up arrow on Linux X11; elsewhere often same as SizeVertical
    SizeBottom, //!< Down arrow on Linux X11; elsewhere often same as SizeVertical
    SizeTopLeft, //!< Top-left corner on Linux X11; elsewhere often same as SizeTopLeftBottomRight
    SizeBottomRight, //!< Bottom-right on Linux X11; elsewhere often same as SizeTopLeftBottomRight
    SizeBottomLeft, //!< Bottom-left on Linux X11; elsewhere often same as SizeBottomLeftTopRight
    SizeTopRight, //!< Top-right on Linux X11; elsewhere often same as SizeBottomLeftTopRight
    SizeAll, //!< Move / all directions
    Cross, //!< Crosshair
    Help, //!< Help
    NotAllowed, //!< Operation not allowed
};

/**
 * Parameters for a RGBA8 texture cursor (row-major, top-left origin, straight alpha).
 * Hotspot is in **image pixels** (0,0 = top-left).
 */
struct CursorImageParams {
    const std::uint8_t * rgba_pixels{nullptr};
    int width{0};
    int height{0};
    /** Bytes per row; 0 means `width * 4`. */
    int stride_bytes{0};
    int hotspot_x{0};
    int hotspot_y{0};
    /** Optional display size in pixels (0 = use image width/height). */
    int display_width{0};
    int display_height{0};
};

} // namespace Graphics

} // namespace HonkordGL

#endif