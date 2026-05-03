/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — textured 2D sprite (OpenGL 3.3 core)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SPRITE_H
#define HONKORDGL_SPRITE_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <cstdint>
#include <memory>

namespace HonkordGL
{
namespace Graphics
{

class Lines;
class Eclipse;

/**
 * GPU textured quad in window pixel space (origin top-left, y increases downward).
 *
 * **Desktop (X11, Wayland, NetBSD, FreeBSD, Raspberry Pi, Cocoa, Win32):** OpenGL 3.3 core — call with
 * `GpuRenderer::MakeCurrent` on the thread that owns the context.
 * **Android:** OpenGL ES 2.0+ (EGL context from `AttachRendererContext`).
 * **DRM/KMS:** no GPU attach in this library — use `CreateFromRGBA(ApplicationContextSettings&, ...)` or
 * `Draw` skips when `native_platform` is DRM (no textured blit).
 */
class HONKORDGL_API Sprite {
public:
    Sprite() noexcept;
    ~Sprite();

    Sprite(Sprite&& other) noexcept;
    Sprite& operator=(Sprite&& other) noexcept;
    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;

    /**
     * Uploads RGBA8 texture data (row-major, top-left first pixel). Stride is bytes per row; 0 = width*4.
     * Call with GL context current. Destroys any previous GPU resources owned by this sprite.
     */
    HONKORDGL_ND bool CreateFromRGBA(int width, int height, const std::uint8_t * rgba, int strideBytes = 0) noexcept;

    /**
     * Same as `CreateFromRGBA(width, height, …)` but returns false immediately when `ctx` is DRM/KMS
     * (no GL context in this build), so callers can avoid invalid GPU use.
     */
    HONKORDGL_ND bool CreateFromRGBA(
        ApplicationContextSettings& ctx,
        int width,
        int height,
        const std::uint8_t * rgba,
        int strideBytes = 0) noexcept;

    /**
     * Same as `CreateFromRGBA` — RGBA8 row-major buffer (`strideBytes` 0 = width×4).
     * `pixels` may be `unsigned char` or `std::uint8_t` data.
     */
    HONKORDGL_ND bool CreateFromPixelBuffer(
        int width,
        int height,
        unsigned char const * pixels,
        int strideBytes = 0) noexcept;
    HONKORDGL_ND bool CreateFromPixelBuffer(
        ApplicationContextSettings& ctx,
        int width,
        int height,
        unsigned char const * pixels,
        int strideBytes = 0) noexcept;

    /**
     * Loads PNG, JPEG, BMP, GIF, TGA, and other formats supported by stb_image (RGBA upload).
     * `path` is UTF-8 on Linux/macOS; on Windows use ASCII or UTF-8 if the CRT supports it.
     */
    HONKORDGL_ND bool CreateFromImageFile(char const * path) noexcept;
    HONKORDGL_ND bool CreateFromImageFile(ApplicationContextSettings& ctx, char const * path) noexcept;

    void Destroy() noexcept;
    HONKORDGL_ND bool IsValid() const noexcept;

    /** Top-left corner in client pixels. */
    void SetPosition(float x, float y) noexcept;
    /** Drawn size in pixels (defaults to texture size until changed). */
    void SetSize(float width, float height) noexcept;

    /** Counter-clockwise rotation in degrees around the quad center (default 0). */
    void SetRotationDegrees(float degrees) noexcept;
    /** Non-uniform scale relative to the quad center (default 1,1). Values may be negative to flip. */
    void SetScale(float scale_x, float scale_y) noexcept;
    /** Uniform scale relative to the quad center. */
    void SetScale(float uniform) noexcept;

    /**
     * Sets both the destination rectangle on screen and the source sub-rectangle in the texture.
     * `dst` uses `Recti`: `x`,`y` = top-left in client pixels; `w`,`z` = width and height.
     * `src` uses the same layout in **texture texels** (top-left origin, `w`/`z` = width/height).
     * Width and height must be positive. Clears `SetImageFrame` and the mode used by `SetPosition` / `SetSize`.
     */
    void ApplyRects(Recti const& dst, Recti const& src) noexcept;

    /**
     * Uses a sub-rectangle of the texture as the sampled image (internal **frame** in texels).
     * Same `Recti` layout as `ApplyRects` `src`. Clears `ApplyRects` mode; pair with `SetPosition` / `SetSize`
     * for placement. Clamped to the texture.
     */
    void SetImageFrame(Recti const& texelsTopLeftWidthHeight) noexcept;

    /** Samples the full texture again (clears `SetImageFrame`). */
    void ClearImageFrame() noexcept;

    HONKORDGL_ND bool HasImageFrame() const noexcept;
    /** Current frame in texels, or full texture when `HasImageFrame()` is false. */
    HONKORDGL_ND Recti ImageFrame() const noexcept;

    /**
     * Pivot for `SetRotationDegrees` / `SetScale`: offset from the sprite **top-left** in destination
     * pixels (x right, y down). Default is the quad center. Matches `Lines` / `Eclipse` entity pivot
     * when used with `ApplyImageEntityFrom`.
     */
    void SetImagePivot(float localXPx, float localYPx) noexcept;
    void ResetImagePivot() noexcept;
    HONKORDGL_ND bool HasCustomImagePivot() const noexcept;
    HONKORDGL_ND float ImagePivotX() const noexcept;
    HONKORDGL_ND float ImagePivotY() const noexcept;

    /** Copies entity position, rotation, scale, and pivot from `Lines` (same conventions as `Lines::End`). */
    void ApplyImageEntityFrom(Lines const& lines) noexcept;

    /** Copies entity position, rotation, scale, and pivot from `Eclipse` (same conventions as `Eclipse::End`). */
    void ApplyImageEntityFrom(Eclipse const& eclipse) noexcept;

    /**
     * Renders the sprite into the current framebuffer using `ctx` for dimensions
     * (`frame_buffer_width` / `frame_buffer_height`, falling back to `client_rect`).
     */
    void Draw(ApplicationContextSettings& ctx) noexcept;

    /**
     * Begins recording vector shapes (client pixel space, origin top-left, y downward).
     * Clears any previous shapes. Pair with `EndShape`. Polylines use `HonkordGL::Graphics::Lines`.
     */
    void BeginShape() noexcept;

    /** Filled triangle (straight alpha). Only between `BeginShape` and `EndShape`. */
    void TriangleFill(
        float x0,
        float y0,
        float x1,
        float y1,
        float x2,
        float y2,
        float r,
        float g,
        float b,
        float a) noexcept;

    /**
     * Filled circle plus optional ring border (same coordinate space as lines). Use `borderThickness`
     * <= 0 to skip the border; use fill alpha 0 to skip the disk fill (border only).
     * Only between `BeginShape` and `EndShape`.
     */
    void Circle(
        float centerX,
        float centerY,
        float radius,
        float fillR,
        float fillG,
        float fillB,
        float fillA,
        float borderR,
        float borderG,
        float borderB,
        float borderA,
        float borderThickness) noexcept;

    /**
     * Flushes the recorded segments with the current GL context. No-op on DRM / invalid sprite.
     * Call with `GpuRenderer::MakeCurrent` on the owning thread (same as `Draw`).
     */
    void EndShape(ApplicationContextSettings& ctx) noexcept;

    HONKORDGL_ND int TextureWidth() const noexcept;
    HONKORDGL_ND int TextureHeight() const noexcept;
    HONKORDGL_ND float PositionX() const noexcept;
    HONKORDGL_ND float PositionY() const noexcept;
    HONKORDGL_ND float DrawWidth() const noexcept;
    HONKORDGL_ND float DrawHeight() const noexcept;
    HONKORDGL_ND float RotationDegrees() const noexcept;
    HONKORDGL_ND float ScaleX() const noexcept;
    HONKORDGL_ND float ScaleY() const noexcept;

    /** Separating-axis test on the two textured quads (same placement as `Draw`). */
    HONKORDGL_ND bool IntersectsEntityBounds(Sprite const& other) const noexcept;

    /** True if `(x, y)` lies inside the transformed draw quad (client pixels, y down). */
    HONKORDGL_ND bool ContainsClientPoint(float x, float y) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Graphics

} // namespace HonkordGL

#endif