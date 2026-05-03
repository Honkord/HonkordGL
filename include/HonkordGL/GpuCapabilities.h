/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — runtime GPU limits and optional features (public vocabulary; queries live in `src/`)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPUCAPABILITIES_H
#define HONKORDGL_GPUCAPABILITIES_H

#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/WindowApplication.h>

#include <cstddef>
#include <cstdint>

namespace HonkordGL::Graphics {

/**
 * Runtime limits and alignment hints from the active implementation (OpenGL family in this build).
 * All integer fields are best-effort: `0` means “unknown”, “not queried”, or “not applicable on this backend”.
 * After a successful `QueryGpuLimits` with `GpuQueryResult::OK`, fields reflect `glGet*` / equivalent where supported;
 * compare texture allocation sizes against `max_texture_2d_size` rather than compile-time `GL_MAX_*` macros.
 */
struct GpuLimits {
    int max_texture_2d_size{};
    int max_cube_map_texture_size{};
    int max_renderbuffer_size{};
    int max_viewport_width{};
    int max_viewport_height{};
    int max_vertex_attribs{};
    int max_vertex_uniform_vectors{};
    int max_fragment_uniform_vectors{};
    int max_varying_vectors{};
    /** `0` = uniform buffer object path not exposed or unsupported. */
    int uniform_buffer_offset_alignment{};
    /** Desktop GL / GLES3+; `0` if unknown. */
    int max_framebuffer_color_attachments{};
    /** MSAA sample count upper bound; `0` or `1` means effectively no MSAA limit query. */
    int max_msaa_samples{};
    /**
     * GPU timestamp period in nanoseconds per tick (timer query path); `0` = unavailable.
     * Best-effort: drivers may report coarse values.
     */
    std::uint64_t gpu_timestamp_ns_per_tick{};
};

enum class GpuQueryResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    MAKE_CURRENT_FAILED = -8,
};

/**
 * Opt-in capability toggles. Enabling only sets internal bookkeeping when the driver already supports the path;
 * it does not replace full feature detection for your rendering code.
 */
enum class GpuOptionalFeature : std::uint32_t {
    /** Geometry stage available (desktop GL 3.2+ or advertised extension stack). */
    GeometryShaderPipeline = 1u << 0,
    /** Depth textures as sampler inputs (3.0+ or common extension bundles). */
    DepthTextureSampling = 1u << 1,
    /** Multisampled renderbuffers / textures (when `GpuLimits::max_msaa_samples` > 1). */
    MultisampleRenderTargets = 1u << 2,
};

enum class GpuFeatureEnableResult : int {
    OK = 0,
    /** Feature was already recorded as available/enabled. */
    ALREADY_SATISFIED = 1,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED = -2,
    UNSUPPORTED_BACKEND = -3,
    NO_DEVICE = -4,
};

/**
 * Fills `out_limits` with zeros first, then runtime values when `ctx` has an attached shader-capable backend
 * and the context can be made current. Threading: same rules as `GpuRenderer::MakeCurrent`.
 */
HONKORDGL_API HONKORDGL_ND int QueryGpuLimits(ApplicationContextSettings * ctx, GpuLimits * out_limits) noexcept;

HONKORDGL_API HONKORDGL_ND int TryEnableGpuOptionalFeature(
    ApplicationContextSettings * ctx,
    GpuOptionalFeature feature) noexcept;

HONKORDGL_API HONKORDGL_ND bool GpuOptionalFeatureIsEnabled(
    ApplicationContextSettings const * ctx,
    GpuOptionalFeature feature) noexcept;

} // namespace HonkordGL::Graphics

#endif
