/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — CPU software framebuffer with optional GPU-accelerated present
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_SOFTWARERENDERER_H
#define HONKORDGL_SOFTWARERENDERER_H

#include <HonkordGL/Software2DContext.h>
#include <HonkordGL/WindowApplication.h>

#include <memory>

namespace HonkordGL::Graphics {

class GpuRenderer;
class WindowBackend;

/**
 * Owns an internal `Software2DContext` (RGBA8) sized from the bound `ApplicationContextSettings`, and presents
 * either via `WindowBackend::PresentSoftwareFramebuffer` (**CPU**) or, after a successful GPU init, by uploading
 * pixels to a GPU texture and drawing a fullscreen quad (**hardware-accelerated** OpenGL / GLES / EGL path).
 *
 * `InitHardwareAcceleratedPresent` probes raster backends in platform order (`GpuRenderer::CreateBestAvailableExhaustive` —
 * Vulkan, Metal, Direct3D 11 (Windows), OpenGL family, etc.); if the attached API cannot upload textures (e.g. Vulkan/Metal
 * swapchain-only in this build), it detaches and falls back to `GpuRenderer::CreateBestAvailableForCpuFramebufferPresent`
 * (OpenGL / EGL / GLES) so CPU RGBA can be presented on the GPU.
 *
 * `app` and `backend` must outlive this object. A successful GPU init attaches a context to `app` (same as `GpuRenderer`).
 */
class HONKORDGL_API SoftwareRenderer {
public:
    /** Opaque runtime state for optional GPU present path (defined in SoftwareRenderer.cpp). */
    struct GpuPresentState;

    /** Defined in `SoftwareRenderer.cpp` so `GpuPresentState` stays opaque to other TUs. */
    SoftwareRenderer() noexcept;
    SoftwareRenderer(ApplicationContextSettings& app, WindowBackend& backend) noexcept;

    SoftwareRenderer(const SoftwareRenderer&) = delete;
    SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;
    SoftwareRenderer(SoftwareRenderer&& other) noexcept;
    SoftwareRenderer& operator=(SoftwareRenderer&& other) noexcept;
    ~SoftwareRenderer() noexcept;

    /** Binds `app` and `backend` for `EnsureSurface` / `Present`. Resets optional GPU present state. */
    void Bind(ApplicationContextSettings& app, WindowBackend& backend) noexcept;

    /** Clears binding; releases CPU pixel storage and GPU present resources if any. */
    void Unbind() noexcept;

    HONKORDGL_ND ApplicationContextSettings * App() const noexcept { return app_; }
    HONKORDGL_ND WindowBackend * Backend() const noexcept { return backend_; }

    /**
     * After the window exists (`window_handle` set), attaches a GPU context and builds the fullscreen blit path.
     * Safe to call once; returns false if no window, DRM, or all attach/compile steps fail (CPU present still works).
     */
    HONKORDGL_ND bool InitHardwareAcceleratedPresent() noexcept;

    /** True after `InitHardwareAcceleratedPresent` succeeded. */
    HONKORDGL_ND bool UsesHardwareAcceleratedPresent() const noexcept;

    /** Short label for the active GPU present backend (`"OPENGL"`, `"EGL"`, …), or `"CPU"` when not using GPU present. */
    HONKORDGL_ND const char * PresentBackendLabel() const noexcept;

    /**
     * Resizes the internal buffer to match `frame_buffer_width` / `frame_buffer_height`, or
     * `client_rect` when framebuffer size is unset or non-positive.
     * @return false if unbound, dimensions invalid, or allocation fails.
     */
    HONKORDGL_ND bool EnsureSurface() noexcept;

    /** Draw target; use after `EnsureSurface()` succeeds. */
    HONKORDGL_ND Software2DContext& Surface() noexcept { return surface_; }
    HONKORDGL_ND const Software2DContext& Surface() const noexcept { return surface_; }

    void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept;

    /**
     * Presents `Surface()` — GPU path when `UsesHardwareAcceleratedPresent()`, otherwise CPU `PresentSoftwareFramebuffer`.
     */
    HONKORDGL_ND bool Present() noexcept;

    /** True when bound, the window handle is set, and the surface matches current width/height with allocated pixels. */
    HONKORDGL_ND bool IsReady() const noexcept;

private:
    void ReleaseGpuPresent() noexcept;

    ApplicationContextSettings * app_{nullptr};
    WindowBackend * backend_{nullptr};
    Software2DContext surface_{};
    std::unique_ptr<GpuPresentState> gpu_{};
};

} // namespace HonkordGL::Graphics

#endif