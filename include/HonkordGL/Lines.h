/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — thick polyline strokes in client pixel space
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_LINES_H
#define HONKORDGL_LINES_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <memory>

namespace HonkordGL
{
namespace Graphics
{

/**
 * GPU thick line segments (OpenGL 3.3 desktop / OpenGL ES 2 Android). No texture required.
 * Coordinates are in **local entity space** until `End`; pivot / scale / rotation / translation
 * map them to framebuffer pixels (origin top-left, y increases downward).
 *
 * **Entity transforms** (`SetEntity*` / `FlipEntity*` / `DilateEntity` / `ResetEntityTransform`) apply to
 * the whole batch and may be changed **outside** as well as inside `Begin`/`End`.
 *
 * **Per-segment style** (`SetNextLineColor`, `SetNextLineThickness`) applies to the next drawn segment
 * only between `Begin` and `End`.
 */
class HONKORDGL_API Lines {
public:
    Lines() noexcept;
    ~Lines();

    Lines(Lines&& other) noexcept;
    Lines& operator=(Lines&& other) noexcept;
    Lines(const Lines&) = delete;
    Lines& operator=(const Lines&) = delete;

    void Destroy() noexcept;

    /** Pivot for scale and rotation in local space (default 0, 0). */
    void SetEntityPivot(float x, float y) noexcept;
    /** Translation in client pixels applied after pivot/scale/rotation. */
    void SetEntityPosition(float x, float y) noexcept;
    /** Counter-clockwise rotation in degrees around `SetEntityPivot`. */
    void SetEntityRotationDegrees(float degrees) noexcept;
    /** Non-uniform scale in local space around the pivot (negative values flip). */
    void SetEntityScale(float scaleX, float scaleY) noexcept;
    void SetEntityScale(float uniform) noexcept;
    /** Uniform scale multiplier (stretches local space — affects stroke thickness). */
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
     * Axis-aligned overlap of stroke bounds vs `other` (client pixels, y down).
     * Each segment is expanded by half its stroke width after the entity transform (matches `End`).
     */
    HONKORDGL_ND bool IntersectsEntityBounds(Lines const& other) const noexcept;

    /** True if `(x, y)` lies within the stroked area of any recorded segment (client pixels). */
    HONKORDGL_ND bool ContainsClientPoint(float x, float y) const noexcept;

    /**
     * Starts recording segments. Clears the path and any previous segments.
     * Entity transform is **not** reset.
     */
    void Begin() noexcept;

    /** Straight alpha RGBA (0..1) for segments produced by the next draw call. */
    void SetNextLineColor(float r, float g, float b, float a) noexcept;

    /** Full stroke width in **pixels** (before entity scale) for the next segment. */
    void SetNextLineThickness(float thicknessPixels) noexcept;

    /** Current point without drawing; required before the first `LineTo` / polar / relative segment. */
    void MoveTo(float x, float y) noexcept;

    /** Segment from the current point to (x, y); updates the current point. */
    void LineTo(float x, float y) noexcept;

    /** Same as `LineTo(pathX + dx, pathY + dy)`; requires `MoveTo` first. */
    void LineRelative(float dx, float dy) noexcept;

    /**
     * From the current point, a segment of `length` pixels at `angleDegrees`.
     * 0° = +x (right), 90° = +y (down). Requires `MoveTo` first.
     */
    void LinePolar(float length, float angleDegrees) noexcept;

    /**
     * One segment from (x0,y0) to (x1,y1); sets the current point to (x1,y1).
     * Does not require `MoveTo` first.
     */
    void LineSegment(float x0, float y0, float x1, float y1) noexcept;

    /**
     * Uploads and draws recorded segments. No-op on DRM. Call with GL current (same as `Sprite::Draw`).
     */
    void End(ApplicationContextSettings& ctx) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Graphics

} // namespace HonkordGL

#endif