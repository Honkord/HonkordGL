/**
 * HonkordGL — internal: pass clear helper shared by `GpuRenderTarget` and `GpuRenderGraph`
 * (not a public include).
 */

#pragma once

#include <HonkordGL/Config.h>

namespace HonkordGL::Graphics {

class GpuRenderer;
class GpuRenderTarget;
struct GpuRenderPassBeginInfo;

/**
 * Per-target color / depth / stencil clears only. Does not `MakeCurrent` or bind the framebuffer;
 * caller must have set up the draw surface and depth state. Returns `GpuRenderPassEncoderResult` codes.
 */
HONKORDGL_API int GlApplyPassClears(
    GpuRenderer & gpu,
    GpuRenderTarget const * target,
    GpuRenderPassBeginInfo const & info) noexcept;

} // namespace HonkordGL::Graphics
