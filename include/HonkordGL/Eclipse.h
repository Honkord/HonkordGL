/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — filled ellipses and circles (GPU, no texture)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_ECLIPSE_H
#define HONKORDGL_ECLIPSE_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <memory>

namespace HonkordGL::Graphics
{

/**
 * Renders **ellipses and circles** with optional stroke ring (OpenGL 3.3 desktop / OpenGL ES 2 Android).
 * Coordinates are in **local entity space** until `End`; the same entity transform model as `Lines`
 * maps them to framebuffer pixels (origin top-left, y increases downward).
 *
 * **Entity transforms** apply to the whole batch and may be set outside or inside `Begin`/`End`.
 *
 * **Per-shape settings** (`SetNextFillColor`, `SetNextBorderColor`, `SetNextBorderThickness`, `SetNextSegmentCount`)
 * apply to the next `Circle` / `Ellipse` call between `Begin` and `End`.
 */
class HONKORDGL_API Eclipse {
public:
    Eclipse() noexcept;
    ~Eclipse();

    Eclipse(Eclipse&& other) noexcept;
    Eclipse& operator=(Eclipse&& other) noexcept;
    Eclipse(const Eclipse&) = delete;
    Eclipse& operator=(const Eclipse&) = delete;

    void Destroy() noexcept;

    void SetEntityPivot(float x, float y) noexcept;
    void SetEntityPosition(float x, float y) noexcept;
    void SetEntityRotationDegrees(float degrees) noexcept;
    void SetEntityScale(float scaleX, float scaleY) noexcept;
    void SetEntityScale(float uniform) noexcept;
    void DilateEntity(float uniformScale) noexcept;
    void FlipEntityX() noexcept;
    void FlipEntityY() noexcept;
    void ResetEntityTransform() noexcept;

    HONKORDGL_ND float EntityPivotX() const noexcept;
    HONKORDGL_ND float EntityPivotY() const noexcept;
    HONKORDGL_ND float EntityPositionX() const noexcept;
    HONKORDGL_ND float EntityPositionY() const noexcept;
    HONKORDGL_ND float EntityRotationDegrees() const noexcept;
    HONKORDGL_ND float EntityScaleX() const noexcept;
    HONKORDGL_ND float EntityScaleY() const noexcept;

    /**
     * Axis-aligned bounds overlap for the recorded ellipse batch vs `other` (client pixels, y down).
     * Uses a conservative world-space AABB per shape (union of fill and border silhouettes).
     */
    HONKORDGL_ND bool IntersectsEntityBounds(Eclipse const& other) const noexcept;

    /** True if `(x, y)` lies inside any filled ellipse or border ring of the current batch (client pixels). */
    HONKORDGL_ND bool ContainsClientPoint(float x, float y) const noexcept;

    /** Clears queued shapes. Entity transform is **not** reset. */
    void Begin() noexcept;

    /** Straight alpha fill for the next ellipse/circle. */
    void SetNextFillColor(float r, float g, float b, float a) noexcept;

    /** Straight alpha for the border ring; use with `SetNextBorderThickness` > 0. */
    void SetNextBorderColor(float r, float g, float b, float a) noexcept;

    /** Border thickness in **pixels** (before entity scale); 0 skips the ring. */
    void SetNextBorderThickness(float thicknessPixels) noexcept;

    /**
     * Tessellation count for the next shape (8–256). Higher values yield smoother circles/ellipses.
     * Default is 64.
     */
    void SetNextSegmentCount(int segments) noexcept;

    /** Circle with equal semiaxes (`radius` > 0). */
    void Circle(float centerX, float centerY, float radius) noexcept;

    /**
     * Axis-aligned ellipse in local space, then `rotationDegrees` rotates the axes (counter-clockwise).
     * `radiusX` / `radiusY` are semiaxes (both > 0).
     */
    void Ellipse(float centerX, float centerY, float radiusX, float radiusY, float rotationDegrees = 0.f) noexcept;

    void End(ApplicationContextSettings& ctx) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace HonkordGL::Graphics

#endif