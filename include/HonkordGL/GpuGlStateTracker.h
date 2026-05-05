/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — OpenGL-family immediate-mode state cache (reduces redundant driver calls)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPUGLSTATETRACKER_H
#define HONKORDGL_GPUGLSTATETRACKER_H

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuRenderPassEncoder.h>
#include <HonkordGL/GpuTypes.h>

namespace HonkordGL::Graphics {

class GpuRenderer;
class GpuRenderTarget;

/**
 * Tracks a small slice of GL rasterizer state (framebuffer, viewport, depth, scissor, color write mask).
 * Call `Invalidate` after **external** GL that bypasses this object (third-party code, another tracker, context loss).
 * Vulkan / Metal: not used — those APIs are explicitly driver-owned command streams.
 */
class HONKORDGL_API GpuGlStateTracker {
public:
    GpuGlStateTracker() noexcept = default;

    /** Drop cached values so the next operation always issues GL (except no-ops). */
    void Invalidate() noexcept;

    /** Bind `GL_FRAMEBUFFER` when the handle differs from the cache. */
    void BindFramebuffer(GpuRenderer & gpu, unsigned int framebuffer_object) noexcept;

    /**
     * Off-screen: `count` color attachments (`GL_COLOR_ATTACHMENT0` …). Default framebuffer: pass `-1` to skip
     * `glDrawBuffers` (swapchain draw buffer left as-is).
     */
    void SetDrawBufferCount(GpuRenderer & gpu, int color_attachment_count) noexcept;

    void SetViewport(int x, int y, int width, int height) noexcept;

    void SetDepthState(bool test_enabled, bool depth_write, GpuDepthFunc depth_func) noexcept;

    void SetScissorEnabled(bool enabled, int x, int y, int width, int height) noexcept;

    void SetColorMask(bool red, bool green, bool blue, bool alpha) noexcept;

    /**
     * Full pass setup + clears: framebuffer, draw buffers, viewport, depth state from `info`, then attachment clears.
     * For `target == nullptr` (swapchain), `surface_width` / `surface_height` must be positive for viewport.
     * Returns `GpuRenderPassEncoderResult` codes.
     */
    HONKORDGL_ND int BeginPass(
        GpuRenderer & gpu,
        GpuRenderTarget * target,
        GpuRenderPassBeginInfo const & info,
        int surface_width,
        int surface_height) noexcept;

    HONKORDGL_ND unsigned int CachedFramebuffer() const noexcept { return cached_fbo_; }

private:
    unsigned int cached_fbo_{};
    bool fbo_known_{false};

    int draw_buffer_count_{-2}; /**< last count passed to `SetDrawBufferCount`; -2 = unknown */
    unsigned int draw_buffer_fbo_{}; /**< framebuffer `draw_buffer_count_` was issued for */

    int vp_x_{};
    int vp_y_{};
    int vp_w_{};
    int vp_h_{};
    bool vp_known_{false};

    bool depth_test_{false};
    bool depth_write_{false};
    GpuDepthFunc depth_func_{GpuDepthFunc::LessOrEqual};
    bool depth_known_{false};

    bool scissor_on_{false};
    int scissor_x_{};
    int scissor_y_{};
    int scissor_w_{};
    int scissor_h_{};
    bool scissor_known_{false};

    bool cm_r_{true};
    bool cm_g_{true};
    bool cm_b_{true};
    bool cm_a_{true};
    bool cm_known_{false};
};

} // namespace HonkordGL::Graphics

#endif
