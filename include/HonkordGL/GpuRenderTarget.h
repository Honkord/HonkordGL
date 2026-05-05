/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — off-screen render targets (FBO + textures): MRT, depth, packed depth/stencil
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPURENDERTARGET_H
#define HONKORDGL_GPURENDERTARGET_H

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuTypes.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace HonkordGL::Graphics {

class GpuRenderer;

/** Sized color surface for an FBO attachment (OpenGL family). */
enum class GpuColorFormat : unsigned char {
    Rgba8Unorm = 1,
    Rgba16Float = 2,
    R11G11B10Float = 3,
};

/** Depth / stencil storage for an FBO (OpenGL family). */
enum class GpuDepthStencilFormat : unsigned char {
    None = 0,
    Depth32Float = 1,
    Depth24Unorm = 2,
    Depth24Stencil8 = 3,
};

enum class GpuRenderTargetResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    OUT_OF_MEMORY = -3,
    INCOMPLETE_TARGET = -4,
    PROC_LOAD_FAILED = -5,
};

/**
 * Describes an off-screen target (G-buffer style). `color_formats[i]` used for `i < color_count`;
 * unused slots ignored. Driver allocates texture storage; no CPU pixel data.
 */
struct GpuRenderTargetCreateInfo {
    int width{};
    int height{};
    int color_count{1}; /**< 1–8 color attachments */
    GpuColorFormat color_formats[8]{};
    GpuDepthStencilFormat depth_stencil{GpuDepthStencilFormat::Depth24Unorm};
};

/**
 * Owns framebuffer object + texture attachments (OpenGL / GLES when supported).
 * Vulkan / Metal swapchain-only backends: `Create` returns `UNSUPPORTED_BACKEND` — use native APIs
 * (`VulkanIntegration`, `MetalIntegration`) for render passes and pipelines there.
 */
class HONKORDGL_API GpuRenderTarget {
public:
    GpuRenderTarget() noexcept = default;
    GpuRenderTarget(GpuRenderTarget const &) = delete;
    GpuRenderTarget & operator=(GpuRenderTarget const &) = delete;
    GpuRenderTarget(GpuRenderTarget &&) noexcept;
    GpuRenderTarget & operator=(GpuRenderTarget &&) noexcept;
    ~GpuRenderTarget() noexcept;

    /**
     * Builds FBO with color textures + optional depth (and stencil when `Depth24Stencil8`).
     * Requires current GL/GLES context (`GpuRenderer::MakeCurrent`); honors `GpuRendererMode` the same as other helpers.
     */
    static HONKORDGL_ND int Create(
        GpuRenderer & gpu,
        GpuRenderTargetCreateInfo const & info,
        std::unique_ptr<GpuRenderTarget> * out) noexcept;

    /** Width / height of attachments. */
    HONKORDGL_ND int Width() const noexcept { return width_; }
    HONKORDGL_ND int Height() const noexcept { return height_; }
    HONKORDGL_ND int ColorCount() const noexcept { return color_count_; }

    HONKORDGL_ND GpuDepthStencilFormat DepthStencilFormat() const noexcept { return desc_.depth_stencil; }

    /** Texture names for `glBindTexture` / sampler bindings (`0` if unused). */
    HONKORDGL_ND GpuObjectName ColorTexture(int index) const noexcept;
    HONKORDGL_ND GpuObjectName DepthStencilTexture() const noexcept;

    /** OpenGL framebuffer object name (`0` if destroyed). */
    HONKORDGL_ND unsigned int FramebufferObject() const noexcept { return fbo_; }

    /**
     * Deletes GL attachments; calls `MakeCurrent()` on `gpu` first.
     * Prefer this before destruction when the creating context may not be current.
     */
    void Destroy(GpuRenderer & gpu) noexcept;

    /** Recreates attachments at new size (same formats). */
    HONKORDGL_ND int Resize(GpuRenderer & gpu, int width, int height) noexcept;

private:
    void DestroyGl() noexcept;
    int BuildGl(GpuRenderer & gpu, GpuRenderTargetCreateInfo const & info) noexcept;

    int width_{};
    int height_{};
    int color_count_{};
    GpuRenderTargetCreateInfo desc_{};

    std::uint32_t fbo_{};
    GpuObjectName colors_[8]{};
    GpuObjectName depth_tex_{};
};

} // namespace HonkordGL::Graphics

#endif
