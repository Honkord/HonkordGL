/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — CPU software 2D framebuffer (RGBA8)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SOFTWARE2DCONTEXT_H
#define HONKORDGL_SOFTWARE2DCONTEXT_H

#include <HonkordGL/Config.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace HonkordGL::Graphics {

/**
 * Software (CPU) 2D rendering surface: packed **RGBA8**, row-major, top-left origin.
 * Pixel layout per pixel: `R, G, B, A` (straight alpha; drawing ops replace destination unless documented).
 * Independent of `ApplicationContextSettings` / GPU `RendererContext` — upload to GPU or blit to a window separately if needed.
 * @see SoftwareRenderer — bind to `ApplicationContextSettings` + `WindowBackend` and call `Present()` (no GPU).
 * @see SoftwareBlitCollector — queue `SoftwareBlit` records and `Apply` into this surface.
 */
class HONKORDGL_API Software2DContext {
public:
    Software2DContext() noexcept;
    ~Software2DContext();

    Software2DContext(Software2DContext&& other) noexcept;
    Software2DContext& operator=(Software2DContext&& other) noexcept;
    Software2DContext(const Software2DContext&) = delete;
    Software2DContext& operator=(const Software2DContext&) = delete;

    explicit Software2DContext(int width, int height);

    /** (Re)allocates the buffer. Zero or negative dimensions clear storage. @return false only on allocation failure. */
    HONKORDGL_ND bool Resize(int width, int height) noexcept;

    void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

    HONKORDGL_ND int Width() const noexcept;
    HONKORDGL_ND int Height() const noexcept;
    /** Bytes between consecutive rows (`Width() * 4` when non-empty). */
    HONKORDGL_ND int StrideBytes() const noexcept;
    HONKORDGL_ND std::size_t ByteSize() const noexcept;

    HONKORDGL_ND std::uint8_t * Pixels() noexcept;
    HONKORDGL_ND const std::uint8_t * Pixels() const noexcept;

    void SetPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

    /** Axis-aligned rectangle; clipped to the buffer. */
    void FillRect(int x, int y, int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

    /** Fills half-open horizontal interval [x0, x1) on row y. Clipped. */
    void FillSpan(int y, int x0, int x1, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace HonkordGL::Graphics

#endif