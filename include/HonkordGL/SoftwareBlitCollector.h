/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — batched software blit descriptors (RGBA8)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SOFTWAREBLITCOLLECTOR_H
#define HONKORDGL_SOFTWAREBLITCOLLECTOR_H

#include <HonkordGL/Config.h>
#include <HonkordGL/Software2DContext.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace HonkordGL::Graphics {

/**
 * Fixed 256-entry RGBA8 palette for indexed (8-bit) blits.
 * Entry `i` maps to bytes `[i*4 .. i*4+3]` as R, G, B, A.
 */
struct HONKORDGL_API SoftwarePalette {
    static constexpr int kEntryCount = 256;

    std::uint8_t rgba[static_cast<std::size_t>(kEntryCount) * 4u]{};

    void Set(std::uint8_t index, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept
    {
        std::uint8_t * const p = &rgba[static_cast<std::size_t>(index) * 4u];
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
    }

    HONKORDGL_ND const std::uint8_t * EntryRGBA(std::uint8_t index) const noexcept
    {
        return &rgba[static_cast<std::size_t>(index) * 4u];
    }
};

/** Intended output dynamic range for the batched blits (metadata for display / GPU upload; `Apply` is still RGBA8 SDR-style). */
enum class CollectorDynamicRange : std::uint8_t {
    SDR = 0,
    HDR = 1,
};

/** Tonemap operator hint when mapping HDR → SDR (metadata only; not executed by `SoftwareBlitCollector::Apply`). */
enum class CollectorTonemap : std::uint8_t {
    None = 0,
    Reinhard = 1,
    AcesFilmic = 2,
    Hable = 3,
};

/** Snapshot of `SoftwareBlitCollector` display / transform hints (see getters on the class). */
struct SoftwareBlitCollectorProperties {
    CollectorDynamicRange dynamic_range{CollectorDynamicRange::SDR};
    CollectorTonemap tonemap{CollectorTonemap::None};
    int hotspot_x{0};
    int hotspot_y{0};
    /** Counter-clockwise degrees; metadata for compositors (not applied by `Apply`). */
    float rotation_degrees{0.f};
    /** Copy of the palette associated with the collector (indexed blits use this on `Apply`). */
    SoftwarePalette palette{};
};

/** Pixel layout for a `SoftwareBlit` source buffer. */
enum class SoftwareBlitFormat : std::uint8_t {
    /** 4 bytes per pixel: R, G, B, A (same as `Software2DContext`). */
    RGBA8 = 0,
    /** 1 byte per pixel: palette index; expanded via `SoftwareBlitCollector`’s palette on `Apply`. */
    Indexed8 = 1,
};

/**
 * One **replace** blit from a source buffer into a `Software2DContext` when `SoftwareBlitCollector::Apply` runs.
 * Row-major, top-left origin.
 *
 * **RGBA8:** `src_stride_bytes` ≥ `width * 4`; sub-rectangle `[src_x, src_x+width) × [src_y, src_y+height)` in pixel coords.
 * **Indexed8:** `src_stride_bytes` ≥ `width`; each source byte is a palette index; same sub-rectangle semantics in **index** space.
 */
struct SoftwareBlit {
    const std::uint8_t * src_pixels{nullptr};
    int src_stride_bytes{0};
    int src_x{0};
    int src_y{0};
    int width{0};
    int height{0};
    int dst_x{0};
    int dst_y{0};
    SoftwareBlitFormat format{SoftwareBlitFormat::RGBA8};
};

/**
 * Queues `SoftwareBlit` entries and applies them in order to a destination `Software2DContext` (clip to destination bounds).
 * Use this to batch many small blits into one pass over the framebuffer.
 *
 * `Add` references **caller-owned** pixel memory (valid until `Apply`, `Clear`, or `FreePixels`).
 * `AddOwned` transfers a heap buffer to the collector; `FreePixels` / `Clear` / destructor release it.
 *
 * **SDR/HDR, tonemap, hotspot, rotation** are stored as **hints** for downstream rendering (GPU, swapchain, or a future
 * extended blitter). The current `Apply` path copies untransformed RGBA8 and does not perform tonemapping or rotation.
 *
 * For a **view-only** variant (no `AddOwned`, external buffers only), see `SoftwareBlitCollectorAlt`.
 */
class HONKORDGL_API SoftwareBlitCollector {
public:
    SoftwareBlitCollector() noexcept = default;

    /**
     * @return true when this library build includes the alternate batched collector (`SoftwareBlitCollectorAlt`).
     * Controlled by `HONKORDGL_SOFTWARE_BLIT_COLLECTOR_ALT_AVAILABLE` in `Config.h`.
     */
    static constexpr bool HasAlternateCollectorVersion() noexcept
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
    HONKORDGL_ND bool IsSDR() const noexcept { return dynamic_range_ == CollectorDynamicRange::SDR; }
    HONKORDGL_ND bool IsHDR() const noexcept { return dynamic_range_ == CollectorDynamicRange::HDR; }

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

    /** Palette used when applying blits with `SoftwareBlitFormat::Indexed8`. */
    void SetPalette(SoftwarePalette p) noexcept { palette_ = p; }
    HONKORDGL_ND const SoftwarePalette & Palette() const noexcept { return palette_; }
    HONKORDGL_ND SoftwarePalette & Palette() noexcept { return palette_; }

    /** Drops all queued blits and frees any buffers added with `AddOwned` (same as `FreePixels`). */
    void Clear() noexcept;

    /** Frees collector-owned pixel buffers and removes all blit entries. External `Add` pointers are simply forgotten. */
    void FreePixels() noexcept;

    void Reserve(std::size_t n);

    HONKORDGL_ND std::size_t Count() const noexcept { return ops_.size(); }

    /** Appends a blit; skipped if `width`/`height` ≤ 0 or `src_pixels` is null. */
    void Add(SoftwareBlit op);

    /**
     * Appends a blit from `pixels`. RGBA8: `width * height * 4` bytes. Indexed8: `width * height` bytes (indices).
     * Ownership transfers to the collector; released on `FreePixels` / `Clear` / destruction.
     */
    void AddOwned(
        std::unique_ptr<std::uint8_t[]> pixels,
        int width,
        int height,
        int dst_x,
        int dst_y,
        SoftwareBlitFormat fmt = SoftwareBlitFormat::RGBA8);

    /** @return Read-only view of queued blits (order preserved). */
    HONKORDGL_ND const SoftwareBlit * Data() const noexcept { return ops_.empty() ? nullptr : ops_.data(); }

    /**
     * Copies all queued blits into `dst` with **replace** semantics, clipping to `dst`’s bounds.
     * @return false if `dst` has no pixels; true otherwise (individual blits may no-op if invalid).
     */
    HONKORDGL_ND bool Apply(Software2DContext& dst) const noexcept;

private:
    std::vector<SoftwareBlit> ops_;
    /** Parallel to `ops_`: non-null means this entry’s `src_pixels` is owned and freed on clear. */
    std::vector<std::unique_ptr<std::uint8_t[]>> owned_;

    CollectorDynamicRange dynamic_range_{CollectorDynamicRange::SDR};
    CollectorTonemap tonemap_{CollectorTonemap::None};
    int hotspot_x_{0};
    int hotspot_y_{0};
    float rotation_degrees_{0.f};
    SoftwarePalette palette_{};
};

} // namespace HonkordGL::Graphics

#endif