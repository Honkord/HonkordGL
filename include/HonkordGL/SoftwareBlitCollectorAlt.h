/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — alternate batched software blit collector (view / external buffers only)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SOFTWAREBLITCOLLECTORALT_H
#define HONKORDGL_SOFTWAREBLITCOLLECTORALT_H

#include <HonkordGL/Config.h>
#include <HonkordGL/SoftwareBlitCollector.h>

#include <cstddef>
#include <vector>

namespace HonkordGL::Graphics {

/**
 * Alternate blit batcher: same `SoftwareBlit` / palette / `Apply` behavior as `SoftwareBlitCollector`,
 * but **only** stores external `Add` references — no `AddOwned`, no internal pixel allocation.
 * Use when all source buffers are owned elsewhere for the lifetime until `Apply` or `Clear`.
 */
class HONKORDGL_API SoftwareBlitCollectorAlt {
public:
    SoftwareBlitCollectorAlt() noexcept = default;

    /** @return Same as `SoftwareBlitCollector::HasAlternateCollectorVersion()` for this build. */
    static constexpr bool IsAvailable() noexcept
    {
#if HONKORDGL_SOFTWARE_BLIT_COLLECTOR_ALT_AVAILABLE
        return true;
#else
        return false;
#endif
    }

    HONKORDGL_ND SoftwareBlitCollectorProperties Properties() const noexcept;
    void SetProperties(SoftwareBlitCollectorProperties p) noexcept;

    void SetDynamicRange(CollectorDynamicRange r) noexcept { dynamic_range_ = r; }
    HONKORDGL_ND CollectorDynamicRange DynamicRange() const noexcept { return dynamic_range_; }

    void SetTonemap(CollectorTonemap t) noexcept { tonemap_ = t; }
    HONKORDGL_ND CollectorTonemap Tonemap() const noexcept { return tonemap_; }

    void SetHotspot(int x, int y) noexcept
    {
        hotspot_x_ = x;
        hotspot_y_ = y;
    }
    HONKORDGL_ND int HotspotX() const noexcept { return hotspot_x_; }
    HONKORDGL_ND int HotspotY() const noexcept { return hotspot_y_; }

    void SetRotationDegrees(float degrees) noexcept { rotation_degrees_ = degrees; }
    HONKORDGL_ND float RotationDegrees() const noexcept { return rotation_degrees_; }

    void SetPalette(SoftwarePalette p) noexcept { palette_ = p; }
    HONKORDGL_ND const SoftwarePalette & Palette() const noexcept { return palette_; }
    HONKORDGL_ND SoftwarePalette & Palette() noexcept { return palette_; }

    /** Drops all queued blits (no heap pixel buffers to free). */
    void Clear() noexcept;

    void Reserve(std::size_t n);

    HONKORDGL_ND std::size_t Count() const noexcept { return ops_.size(); }

    /** Appends a blit; skipped if invalid. Caller keeps `src_pixels` valid until `Apply` or `Clear`. */
    void Add(SoftwareBlit op);

    HONKORDGL_ND const SoftwareBlit * Data() const noexcept { return ops_.empty() ? nullptr : ops_.data(); }

    HONKORDGL_ND bool Apply(Software2DContext& dst) const noexcept;

private:
    std::vector<SoftwareBlit> ops_;

    CollectorDynamicRange dynamic_range_{CollectorDynamicRange::SDR};
    CollectorTonemap tonemap_{CollectorTonemap::None};
    int hotspot_x_{0};
    int hotspot_y_{0};
    float rotation_degrees_{0.f};
    SoftwarePalette palette_{};
};

} // namespace HonkordGL::Graphics

#endif