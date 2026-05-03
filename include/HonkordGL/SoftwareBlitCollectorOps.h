/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — shared software blit apply (used by collectors)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SOFTWAREBLITCOLLECTOROPS_H
#define HONKORDGL_SOFTWAREBLITCOLLECTOROPS_H

#include <HonkordGL/SoftwareBlitCollector.h>

#include <algorithm>
#include <cstring>
#include <cstddef>

namespace HonkordGL::Graphics {

/** Apply one clipped blit (RGBA8 or Indexed8 using `pal` for indexed). */
inline void ApplySoftwareBlitOne(const SoftwareBlit& b, Software2DContext& dst, const SoftwarePalette& pal) noexcept
{
    if (b.format == SoftwareBlitFormat::Indexed8) {
        if (!b.src_pixels || b.width <= 0 || b.height <= 0)
            return;
        if (b.src_stride_bytes < b.width)
            return;

        const int dw = dst.Width();
        const int dh = dst.Height();
        if (dw <= 0 || dh <= 0)
            return;

        std::uint8_t * const d0 = dst.Pixels();
        if (!d0)
            return;

        const int dx = b.dst_x;
        const int dy = b.dst_y;
        const int w = b.width;
        const int h = b.height;

        const int clip_x0 = (std::max)(0, dx);
        const int clip_y0 = (std::max)(0, dy);
        const int clip_x1 = (std::min)(dw, dx + w);
        const int clip_y1 = (std::min)(dh, dy + h);
        if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1)
            return;

        const int src_x0 = b.src_x + (clip_x0 - dx);
        const int src_y0 = b.src_y + (clip_y0 - dy);
        const int cw = clip_x1 - clip_x0;
        const int ch = clip_y1 - clip_y0;

        if (src_x0 + cw > b.src_x + w || src_y0 + ch > b.src_y + h)
            return;

        const int dst_stride = dst.StrideBytes();

        for (int row = 0; row < ch; ++row) {
            const std::uint8_t * const srow =
                b.src_pixels + static_cast<std::size_t>(src_y0 + row) * static_cast<std::size_t>(b.src_stride_bytes)
                + static_cast<std::size_t>(src_x0);
            std::uint8_t * const drow =
                d0 + static_cast<std::size_t>(clip_y0 + row) * static_cast<std::size_t>(dst_stride)
                + static_cast<std::size_t>(clip_x0) * 4u;
            for (int col = 0; col < cw; ++col) {
                const std::uint8_t idx = srow[col];
                const std::uint8_t * const pr = pal.EntryRGBA(idx);
                std::uint8_t * const pd = drow + static_cast<std::size_t>(col) * 4u;
                pd[0] = pr[0];
                pd[1] = pr[1];
                pd[2] = pr[2];
                pd[3] = pr[3];
            }
        }
        return;
    }

    if (!b.src_pixels || b.width <= 0 || b.height <= 0)
        return;
    if (b.src_stride_bytes < b.width * 4)
        return;

    const int dw = dst.Width();
    const int dh = dst.Height();
    if (dw <= 0 || dh <= 0)
        return;

    std::uint8_t * const d0 = dst.Pixels();
    if (!d0)
        return;

    const int dx = b.dst_x;
    const int dy = b.dst_y;
    const int w = b.width;
    const int h = b.height;

    const int clip_x0 = (std::max)(0, dx);
    const int clip_y0 = (std::max)(0, dy);
    const int clip_x1 = (std::min)(dw, dx + w);
    const int clip_y1 = (std::min)(dh, dy + h);
    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1)
        return;

    const int src_x0 = b.src_x + (clip_x0 - dx);
    const int src_y0 = b.src_y + (clip_y0 - dy);
    const int cw = clip_x1 - clip_x0;
    const int ch = clip_y1 - clip_y0;

    if (src_x0 + cw > b.src_x + w || src_y0 + ch > b.src_y + h)
        return;
    if ((src_x0 + cw) * 4 > b.src_stride_bytes)
        return;

    const int dst_stride = dst.StrideBytes();
    const std::size_t row_bytes = static_cast<std::size_t>(cw) * 4u;

    for (int row = 0; row < ch; ++row) {
        const std::uint8_t * const srow =
            b.src_pixels + static_cast<std::size_t>(src_y0 + row) * static_cast<std::size_t>(b.src_stride_bytes)
            + static_cast<std::size_t>(src_x0) * 4u;
        std::uint8_t * const drow =
            d0 + static_cast<std::size_t>(clip_y0 + row) * static_cast<std::size_t>(dst_stride)
            + static_cast<std::size_t>(clip_x0) * 4u;
        std::memcpy(drow, srow, row_bytes);
    }
}
/** Apply a sequence of blits in order (caller-owned `SoftwareBlit` pointers). */
inline void ApplySoftwareBlitList(const SoftwareBlit * ops, std::size_t count, Software2DContext& dst, const SoftwarePalette& pal) noexcept
{
    if (!ops || count == 0)
        return;
    for (std::size_t i = 0; i < count; ++i)
        ApplySoftwareBlitOne(ops[i], dst, pal);
}

} // namespace HonkordGL::Graphics

#endif