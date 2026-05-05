/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — ordered multi-pass render graph (OpenGL family) + optional dependency ordering
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPURENDERGRAPH_H
#define HONKORDGL_GPURENDERGRAPH_H

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuGlStateTracker.h>
#include <HonkordGL/GpuRenderPassEncoder.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace HonkordGL::Graphics {

class GpuRenderer;
class GpuRenderTarget;

/** One scheduled pass: optional off-screen `target` or swapchain when null (use `surface_*`). */
struct GpuRenderGraphPass {
    GpuRenderTarget * target{nullptr};
    /** Used when `target == nullptr`: viewport / clears index 0 for the default framebuffer. */
    int surface_width{};
    int surface_height{};
    GpuRenderPassBeginInfo begin{};
    char const * debug_name{nullptr};
};

enum class GpuRenderGraphResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    CYCLIC_DEPENDENCY = -3,
};

/**
 * Multi-pass graph: linear order by default; optional **DAG** edges **prerequisite → dependent**
 * (`AddDependency(prereq, dependent)`). Call `Compile()` after editing passes/edges; `Execute` runs `Compile` if needed.
 */
class HONKORDGL_API GpuRenderGraph {
public:
    GpuRenderGraph() noexcept = default;

    void Clear() noexcept;
    void Reserve(std::size_t pass_capacity);
    void AddPass(GpuRenderGraphPass const & pass);

    /** Invalidates compilation; `dependent` runs after `prerequisite` (indices into `Passes()`). */
    void AddDependency(int prerequisite_pass_index, int dependent_pass_index);

    /**
     * Builds an execution order (topological). Returns `CYCLIC_DEPENDENCY` if the edge set has a cycle.
     */
    HONKORDGL_ND int Compile() noexcept;

    /**
     * Runs passes in compiled order: `MakeCurrent` once, each pass `tracker.BeginPass`, then `per_pass`, then bind FBO 0.
     * `per_pass` may be null to only batch clears/state for profiling.
     */
    HONKORDGL_ND int Execute(
        GpuRenderer & gpu,
        GpuGlStateTracker & tracker,
        void (*per_pass)(void * user_data, int pass_index, GpuRenderGraphPass const & pass),
        void * user_data) noexcept;

    HONKORDGL_ND std::vector<GpuRenderGraphPass> const & Passes() const noexcept { return passes_; }
    HONKORDGL_ND std::vector<int> const & ExecutionOrder() const noexcept { return order_; }
    HONKORDGL_ND bool IsCompiled() const noexcept { return compiled_; }

private:
    std::vector<GpuRenderGraphPass> passes_;
    std::vector<std::pair<int, int>> edges_; /**< (prerequisite, dependent) */
    std::vector<int> order_;
    bool compiled_{false};
};

} // namespace HonkordGL::Graphics

#endif
