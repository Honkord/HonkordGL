/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — portable GPU resource naming (no OpenGL / Direct3D / Vulkan types in the public surface)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_GPUTYPES_H
#define HONKORDGL_GPUTYPES_H

#include <HonkordGL/Config.h>

#include <cstdint>

namespace HonkordGL::Graphics {

/** Opaque handle for shader programs, textures, buffers, etc., in the active HonkordGL graphics backend. */
using GpuObjectName = unsigned int;

/** Logical GPU adapter index for future multi-GPU selection; `0` always means default / primary in this build. */
using GpuDeviceId = std::uint32_t;

/**
 * Selects which adapter information string `GpuRenderer::GetAdapterString` copies.
 * Values are HonkordGL-owned (implementation maps to backend-specific queries internally).
 */
enum class GpuAdapterStringId : unsigned char {
    Vendor = 1,
    Renderer = 2,
    Version = 3,
};

/** Generic GPU buffer binding slot. */
enum class GpuBufferTarget : unsigned int {
    Vertex = 1,
    Index = 2,
};

/** GPU upload hint for `GpuRenderer::UploadBufferData`. */
enum class GpuBufferUsage : unsigned int {
    Static = 1,
    Dynamic = 2,
    Stream = 3,
};

/** Primitive topology for draw calls. */
enum class GpuPrimitive : unsigned int {
    Triangles = 1,
};

/** Index element type for indexed draw calls. */
enum class GpuIndexType : unsigned int {
    UInt16 = 1,
    UInt32 = 2,
};

/**
 * Shader stage for `GpuRenderer::CreateShaderObject` / low-level GLSL compilation.
 * Geometry is unavailable on some OpenGL ES 2 paths (`GpuShaderCompileError::UNSUPPORTED_BACKEND`).
 */
enum class GpuShaderPipelineStage : unsigned char {
    Vertex,
    Fragment,
    Geometry,
};

/**
 * `GpuRenderer` context-binding policy.
 * - **Assisted** (default): most GPU helpers call `MakeCurrent` internally before issuing GL/D3D work.
 * - **Minimal**: caller keeps the rendering context current across calls — fewer `MakeCurrent` transitions,
 *   but incorrect ordering yields undefined driver behavior. Prefer with frame loops that already call
 *   `GpuRenderer::MakeCurrent()` once per frame (or per thread).
 */
enum class GpuRendererMode : unsigned char {
    Assisted = 0,
    Minimal = 1,
};

/** Depth comparison for `GpuRenderPassBeginInfo` / `GpuGlStateTracker` (OpenGL family). */
enum class GpuDepthFunc : unsigned char {
    Never = 1,
    Less = 2,
    Equal = 3,
    LessOrEqual = 4,
    Greater = 5,
    NotEqual = 6,
    GreaterOrEqual = 7,
    DepthAlways = 8,
};

} // namespace HonkordGL::Graphics

#endif
