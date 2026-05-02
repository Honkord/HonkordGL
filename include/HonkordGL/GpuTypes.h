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

/**
 * Selects which adapter information string `GpuRenderer::GetAdapterString` copies
 * (numeric values match common graphics query constants).
 */
enum class GpuAdapterStringId : unsigned int {
    Vendor = 0x1F00,
    Renderer = 0x1F01,
    Version = 0x1F02,
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

} // namespace HonkordGL::Graphics

#endif
