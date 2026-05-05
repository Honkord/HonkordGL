/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GPU rendering pass scope for OpenGL-family (bind FBO, clears, viewport, depth state)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPURENDERPASSENCODER_H
#define HONKORDGL_GPURENDERPASSENCODER_H

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuRenderTarget.h>
#include <HonkordGL/GpuTypes.h>

#include <cstdint>

namespace HonkordGL::Graphics {

class GpuRenderer;

/** Bit mask for `GpuRenderPassBeginInfo::clear_mask`. */
enum GpuRenderPassClearBits : std::uint32_t {
    GPU_CLEAR_DEPTH = 1u << 0,
    GPU_CLEAR_STENCIL = 1u << 1,
    GPU_CLEAR_COLOR0 = 1u << 16,
    GPU_CLEAR_COLOR1 = 1u << 17,
    GPU_CLEAR_COLOR2 = 1u << 18,
    GPU_CLEAR_COLOR3 = 1u << 19,
    GPU_CLEAR_COLOR4 = 1u << 20,
    GPU_CLEAR_COLOR5 = 1u << 21,
    GPU_CLEAR_COLOR6 = 1u << 22,
    GPU_CLEAR_COLOR7 = 1u << 23,
};

/** Per-attachment clear (matches bound target layout). */
struct GpuRenderPassClearColor {
    float r{};
    float g{};
    float b{};
    float a{1.f};
};

/**
 * Begin a pass into `target`: binds FBO, sets viewport to full target, optional clears, depth test toggle.
 * OpenGL-family only; caller issues draws with `GpuRenderer` / raw GL until `GpuRenderPassEncoderGl::End`.
 */
struct GpuRenderPassBeginInfo {
    std::uint32_t clear_mask{0}; /**< `GpuRenderPassClearBits`; 0 = load existing contents */
    GpuRenderPassClearColor color_clears[8]{};
    float depth_clear{1.f};
    int stencil_clear{};
    int enable_depth_test{1};
    int depth_write{1}; /**< `glDepthMask` when depth test enabled */
    GpuDepthFunc depth_func{GpuDepthFunc::LessOrEqual};
};

enum class GpuRenderPassEncoderResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
};

/**
 * RAII-style GL pass encoder: **Begin** in constructor, **End** in destructor, or call `End()` explicitly.
 * Does not batch GPU work beyond bind/clear/state — the driver executes commands immediately (immediate-mode GL).
 */
class HONKORDGL_API GpuRenderPassEncoderGl {
public:
    GpuRenderPassEncoderGl() noexcept = default;
    GpuRenderPassEncoderGl(
        GpuRenderer & gpu,
        GpuRenderTarget & target,
        GpuRenderPassBeginInfo const & info) noexcept;
    GpuRenderPassEncoderGl(GpuRenderPassEncoderGl const &) = delete;
    GpuRenderPassEncoderGl & operator=(GpuRenderPassEncoderGl const &) = delete;
    GpuRenderPassEncoderGl(GpuRenderPassEncoderGl && other) noexcept;
    GpuRenderPassEncoderGl & operator=(GpuRenderPassEncoderGl && other) noexcept;
    ~GpuRenderPassEncoderGl() noexcept;

    void End() noexcept;

    /** Optional scissor rectangle in framebuffer pixels (disabled if width/height <= 0). */
    void SetScissor(int x, int y, int width, int height) noexcept;

    HONKORDGL_ND int LastResult() const noexcept { return last_result_; }

private:
    GpuRenderer * gpu_{nullptr};
    bool active_{false};
    bool scissor_enabled_{false};
    int last_result_{0};
};

/**
 * Metal: render passes are encoded with `MTLRenderCommandEncoder` + `MTLRenderPassDescriptor` (driver-owned).
 * Use `MetalIntegrationGetNativeHandles` and Apple’s Metal API — no HonkordGL encoder wrapper in this build.
 *
 * Vulkan: use `VkRenderPass`, `VkFramebuffer`, `vkCmdBeginRenderPass` — expose via `VulkanIntegration` + loader.
 *
 * D3D11: use `ID3D11DeviceContext::OMSetRenderTargets` + `ClearDepthStencilView` — expose via `Direct3D11IntegrationGetNativeHandles`.
 */

} // namespace HonkordGL::Graphics

#endif
