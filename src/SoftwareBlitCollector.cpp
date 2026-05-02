/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — SoftwareBlitCollector implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/SoftwareBlitCollector.h>
#include <HonkordGL/SoftwareBlitCollectorOps.h>

#include <utility>

namespace HonkordGL::Graphics {

SoftwareBlitCollectorProperties SoftwareBlitCollector::Properties() const noexcept
{
    SoftwareBlitCollectorProperties p{};
    p.dynamic_range = dynamic_range_;
    p.tonemap = tonemap_;
    p.hotspot_x = hotspot_x_;
    p.hotspot_y = hotspot_y_;
    p.rotation_degrees = rotation_degrees_;
    p.palette = palette_;
    return p;
}
void SoftwareBlitCollector::SetProperties(SoftwareBlitCollectorProperties p) noexcept
{
    dynamic_range_ = p.dynamic_range;
    tonemap_ = p.tonemap;
    hotspot_x_ = p.hotspot_x;
    hotspot_y_ = p.hotspot_y;
    rotation_degrees_ = p.rotation_degrees;
    palette_ = p.palette;
}

void SoftwareBlitCollector::FreePixels() noexcept
{
    ops_.clear();
    owned_.clear();
}
void SoftwareBlitCollector::Clear() noexcept
{
    FreePixels();
}
void SoftwareBlitCollector::Reserve(std::size_t n)
{
    ops_.reserve(n);
    owned_.reserve(n);
}
void SoftwareBlitCollector::Add(SoftwareBlit op)
{
    if (!op.src_pixels || op.width <= 0 || op.height <= 0)
        return;
    if (op.format == SoftwareBlitFormat::RGBA8) {
        if (op.src_stride_bytes < op.width * 4)
            return;
    } else {
        if (op.src_stride_bytes < op.width)
            return;
    }
    ops_.push_back(op);
    try {
        owned_.push_back(nullptr);
    } catch (...) {
        ops_.pop_back();
        throw;
    }
}
void SoftwareBlitCollector::AddOwned(std::unique_ptr<std::uint8_t[]> pixels, int width, int height, int dst_x, int dst_y, SoftwareBlitFormat fmt)
{
    if (!pixels || width <= 0 || height <= 0)
        return;

    SoftwareBlit b{};
    b.src_pixels = pixels.get();
    b.src_x = 0;
    b.src_y = 0;
    b.width = width;
    b.height = height;
    b.dst_x = dst_x;
    b.dst_y = dst_y;
    b.format = fmt;

    if (fmt == SoftwareBlitFormat::RGBA8) {
        b.src_stride_bytes = width * 4;
    } else {
        b.src_stride_bytes = width;
    }

    ops_.push_back(b);
    try {
        owned_.push_back(std::move(pixels));
    } catch (...) {
        ops_.pop_back();
        throw;
    }
}
bool SoftwareBlitCollector::Apply(Software2DContext& dst) const noexcept
{
    if (dst.Width() <= 0 || dst.Height() <= 0 || !dst.Pixels())
        return false;
    ApplySoftwareBlitList(ops_.data(), ops_.size(), dst, palette_);
    return true;
}

} // namespace HonkordGL::Graphics