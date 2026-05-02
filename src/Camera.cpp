/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Camera2D implementation
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/Camera.h>

#include <HonkordGL/Eclipse.h>
#include <HonkordGL/Lines.h>
#include <HonkordGL/Sprite.h>

#include <algorithm>

namespace HonkordGL::Graphics
{

Camera2D::Camera2D() noexcept
{
    Reset();
}

void Camera2D::Reset() noexcept
{
    focus_x_ = 0.f;
    focus_y_ = 0.f;
    rotation_degrees_ = 0.f;
    zoom_ = 1.f;
    viewport_norm_ = Rectf{0.f, 0.f, 1.f, 1.f};
    output_rect_px_ = Rectf{0.f, 0.f, 0.f, 0.f};
    tint_r_ = tint_g_ = tint_b_ = 1.f;
    tint_a_ = 1.f;
    tint_mix_ = 0.f;
    tracked_sprite_ = nullptr;
    tracked_lines_ = nullptr;
    tracked_eclipse_ = nullptr;
}

void Camera2D::EncapsulateFramebuffer(ApplicationContextSettings const& ctx) noexcept
{
    const int fbw = ctx.frame_buffer_width > 0 ? ctx.frame_buffer_width : ctx.client_rect.w;
    const int fbh = ctx.frame_buffer_height > 0 ? ctx.frame_buffer_height : ctx.client_rect.z;
    const float w = static_cast<float>(std::max(0, fbw));
    const float h = static_cast<float>(std::max(0, fbh));
    output_rect_px_ = Rectf{0.f, 0.f, w, h};
}

void Camera2D::SetFocus(float x, float y) noexcept
{
    focus_x_ = x;
    focus_y_ = y;
    tracked_sprite_ = nullptr;
    tracked_lines_ = nullptr;
    tracked_eclipse_ = nullptr;
}

void Camera2D::SetMainFocus(Sprite const& sprite) noexcept
{
    tracked_sprite_ = &sprite;
    tracked_lines_ = nullptr;
    tracked_eclipse_ = nullptr;
    focus_x_ = sprite.PositionX();
    focus_y_ = sprite.PositionY();
}

void Camera2D::SetMainFocus(Lines const& lines) noexcept
{
    tracked_sprite_ = nullptr;
    tracked_lines_ = &lines;
    tracked_eclipse_ = nullptr;
    focus_x_ = lines.EntityPositionX();
    focus_y_ = lines.EntityPositionY();
}

void Camera2D::SetMainFocus(Eclipse const& eclipse) noexcept
{
    tracked_sprite_ = nullptr;
    tracked_lines_ = nullptr;
    tracked_eclipse_ = &eclipse;
    focus_x_ = eclipse.EntityPositionX();
    focus_y_ = eclipse.EntityPositionY();
}

void Camera2D::SyncFocusFromTrackedEntity() noexcept
{
    if (tracked_sprite_) {
        focus_x_ = tracked_sprite_->PositionX();
        focus_y_ = tracked_sprite_->PositionY();
        return;
    }
    if (tracked_lines_) {
        focus_x_ = tracked_lines_->EntityPositionX();
        focus_y_ = tracked_lines_->EntityPositionY();
        return;
    }
    if (tracked_eclipse_) {
        focus_x_ = tracked_eclipse_->EntityPositionX();
        focus_y_ = tracked_eclipse_->EntityPositionY();
    }
}

void Camera2D::MoveBy(float slope_dx, float slope_dy) noexcept
{
    focus_x_ += slope_dx;
    focus_y_ += slope_dy;
}

void Camera2D::SetViewportRotationDegrees(float degrees) noexcept
{
    rotation_degrees_ = degrees;
}

void Camera2D::RotateViewportBy(float delta_degrees) noexcept
{
    rotation_degrees_ += delta_degrees;
}

void Camera2D::SetViewportRegionNormalized(float x, float y, float w, float h) noexcept
{
    viewport_norm_.x = std::clamp(x, 0.f, 1.f);
    viewport_norm_.y = std::clamp(y, 0.f, 1.f);
    viewport_norm_.w = std::clamp(w, 0.f, 1.f);
    viewport_norm_.z = std::clamp(h, 0.f, 1.f);
}

void Camera2D::SetViewportRegionNormalized(Rectf const& region) noexcept
{
    SetViewportRegionNormalized(region.x, region.y, region.w, region.z);
}

void Camera2D::MapViewportToRect(float x, float y, float w, float h) noexcept
{
    output_rect_px_.x = x;
    output_rect_px_.y = y;
    output_rect_px_.w = std::max(0.f, w);
    output_rect_px_.z = std::max(0.f, h);
}

void Camera2D::MapViewportToRect(Rectf const& rect) noexcept
{
    MapViewportToRect(rect.x, rect.y, rect.w, rect.z);
}

void Camera2D::SetTintMix(float r, float g, float b, float a, float mix) noexcept
{
    tint_r_ = std::clamp(r, 0.f, 1.f);
    tint_g_ = std::clamp(g, 0.f, 1.f);
    tint_b_ = std::clamp(b, 0.f, 1.f);
    tint_a_ = std::clamp(a, 0.f, 1.f);
    tint_mix_ = std::clamp(mix, 0.f, 1.f);
}

void Camera2D::ClearTintMix() noexcept
{
    tint_r_ = tint_g_ = tint_b_ = 1.f;
    tint_a_ = 1.f;
    tint_mix_ = 0.f;
}

void Camera2D::SetZoom(float zoom) noexcept
{
    if (zoom > 1e-6f)
        zoom_ = zoom;
}

void Camera2D::ZoomIn(float factor) noexcept
{
    if (factor > 1e-6f)
        zoom_ *= factor;
}

void Camera2D::ZoomOut(float factor) noexcept
{
    if (factor > 1e-6f)
        zoom_ /= factor;
}

std::vector<Camera2D> Camera2D::BuildSplitScreen(unsigned int count, Camera2D const& base) noexcept
{
    std::vector<Camera2D> out{};
    if (count == 0)
        return out;
    out.resize(count, base);
    const float slice = 1.f / static_cast<float>(count);
    for (unsigned int i = 0; i < count; ++i) {
        out[i].SetViewportRegionNormalized(slice * static_cast<float>(i), 0.f, slice, 1.f);
    }
    return out;
}

float Camera2D::FocusX() const noexcept
{
    return focus_x_;
}

float Camera2D::FocusY() const noexcept
{
    return focus_y_;
}

float Camera2D::ViewportRotationDegrees() const noexcept
{
    return rotation_degrees_;
}

float Camera2D::Zoom() const noexcept
{
    return zoom_;
}

Rectf Camera2D::ViewportRegionNormalized() const noexcept
{
    return viewport_norm_;
}

Rectf Camera2D::OutputRectPixels() const noexcept
{
    return output_rect_px_;
}

float Camera2D::TintR() const noexcept
{
    return tint_r_;
}

float Camera2D::TintG() const noexcept
{
    return tint_g_;
}

float Camera2D::TintB() const noexcept
{
    return tint_b_;
}

float Camera2D::TintA() const noexcept
{
    return tint_a_;
}

float Camera2D::TintMixAmount() const noexcept
{
    return tint_mix_;
}

} // namespace HonkordGL::Graphics