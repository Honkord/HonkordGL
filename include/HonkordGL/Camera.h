/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — 2D camera controller and viewport mapping
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_CAMERA_H
#define HONKORDGL_CAMERA_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <vector>

namespace HonkordGL
{
namespace Graphics
{

class Sprite;
class Lines;
class Eclipse;

/**
 * Camera rectangle + transform in client pixel space (origin top-left, y down).
 *
 * The camera stores:
 * - World center (focus point),
 * - Rotation (counter-clockwise degrees),
 * - Zoom multiplier,
 * - Viewport region in normalized framebuffer space [0,1],
 * - Destination output rectangle in client pixels (for mapping viewport into a target area),
 * - Global tint blend color + amount.
 */
class HONKORDGL_API Camera2D {
public:
    Camera2D() noexcept;

    /** Reset to a full-framebuffer camera with identity transform and no tint. */
    void Reset() noexcept;

    /**
     * Rebind this camera to the active framebuffer size.
     * Uses framebuffer dims when available, otherwise client size.
     */
    void EncapsulateFramebuffer(ApplicationContextSettings const& ctx) noexcept;

    /** Set world focus directly in client/world pixels. */
    void SetFocus(float x, float y) noexcept;

    /** Focus and track a sprite's current top-left position. */
    void SetMainFocus(Sprite const& sprite) noexcept;

    /** Focus and track a line entity's transform position. */
    void SetMainFocus(Lines const& lines) noexcept;

    /** Focus and track an eclipse entity's transform position. */
    void SetMainFocus(Eclipse const& eclipse) noexcept;

    /**
     * Refresh focus from whichever entity is being tracked.
     * Safe to call every frame.
     */
    void SyncFocusFromTrackedEntity() noexcept;

    /** Move camera focus by 2D slope vector delta. */
    void MoveBy(float slope_dx, float slope_dy) noexcept;

    /** Rotate full viewport around camera focus in counter-clockwise degrees. */
    void SetViewportRotationDegrees(float degrees) noexcept;
    void RotateViewportBy(float delta_degrees) noexcept;

    /** Set normalized viewport region inside framebuffer: x,y,w,h in [0,1]. */
    void SetViewportRegionNormalized(float x, float y, float w, float h) noexcept;

    /** Set normalized viewport region using a rectangle. */
    void SetViewportRegionNormalized(Rectf const& region) noexcept;

    /**
     * Set output mapping rectangle in client pixels.
     * The view picture of this camera will map to this destination rectangle.
     */
    void MapViewportToRect(float x, float y, float w, float h) noexcept;

    /** Set output mapping rectangle in client pixels. */
    void MapViewportToRect(Rectf const& rect) noexcept;

    /** Blend the camera image with a color tint (`mix` in [0,1]). */
    void SetTintMix(float r, float g, float b, float a, float mix) noexcept;

    /** Clear tint blending. */
    void ClearTintMix() noexcept;

    /** Set absolute camera zoom (must be > 0). */
    void SetZoom(float zoom) noexcept;

    /** Zoom in by multiplying current zoom. */
    void ZoomIn(float factor) noexcept;

    /** Zoom out by dividing current zoom. */
    void ZoomOut(float factor) noexcept;

    /** Build `count` horizontal split-screen cameras from this camera. */
    HONKORDGL_ND static std::vector<Camera2D> BuildSplitScreen(unsigned int count, Camera2D const& base) noexcept;

    HONKORDGL_ND float FocusX() const noexcept;
    HONKORDGL_ND float FocusY() const noexcept;
    HONKORDGL_ND float ViewportRotationDegrees() const noexcept;
    HONKORDGL_ND float Zoom() const noexcept;
    HONKORDGL_ND Rectf ViewportRegionNormalized() const noexcept;
    HONKORDGL_ND Rectf OutputRectPixels() const noexcept;
    HONKORDGL_ND float TintR() const noexcept;
    HONKORDGL_ND float TintG() const noexcept;
    HONKORDGL_ND float TintB() const noexcept;
    HONKORDGL_ND float TintA() const noexcept;
    HONKORDGL_ND float TintMixAmount() const noexcept;

private:
    float focus_x_{0.f};
    float focus_y_{0.f};
    float rotation_degrees_{0.f};
    float zoom_{1.f};
    Rectf viewport_norm_{0.f, 0.f, 1.f, 1.f};
    Rectf output_rect_px_{0.f, 0.f, 0.f, 0.f};

    float tint_r_{1.f};
    float tint_g_{1.f};
    float tint_b_{1.f};
    float tint_a_{1.f};
    float tint_mix_{0.f};

    Sprite const * tracked_sprite_{nullptr};
    Lines const * tracked_lines_{nullptr};
    Eclipse const * tracked_eclipse_{nullptr};
};

} // namespace Graphics
} // namespace HonkordGL

#endif