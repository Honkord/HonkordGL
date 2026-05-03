/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — SoftwareBlitCollectorAlt implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/SoftwareBlitCollectorAlt.h>
#include <HonkordGL/SoftwareBlitCollectorOps.h>

namespace HonkordGL::Graphics {

SoftwareBlitCollectorProperties SoftwareBlitCollectorAlt::Properties() const noexcept
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
void SoftwareBlitCollectorAlt::SetProperties(SoftwareBlitCollectorProperties p) noexcept
{
    dynamic_range_ = p.dynamic_range;
    tonemap_ = p.tonemap;
    hotspot_x_ = p.hotspot_x;
    hotspot_y_ = p.hotspot_y;
    rotation_degrees_ = p.rotation_degrees;
    palette_ = p.palette;
}

void SoftwareBlitCollectorAlt::Clear() noexcept
{
    ops_.clear();
}

void SoftwareBlitCollectorAlt::Reserve(std::size_t n)
{
    ops_.reserve(n);
}

void SoftwareBlitCollectorAlt::Add(SoftwareBlit op)
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
}

bool SoftwareBlitCollectorAlt::Apply(Software2DContext& dst) const noexcept
{
    if (dst.Width() <= 0 || dst.Height() <= 0 || !dst.Pixels())
        return false;
    ApplySoftwareBlitList(ops_.data(), ops_.size(), dst, palette_);
    return true;
}

} // namespace HonkordGL::Graphics