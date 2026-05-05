/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — GLSL generation + shader program compilation helpers
 * Copyright (C) 2026 Honkord
 */

/**
 * @file GpuShaderCompiler.h
 * @brief Source-level GLSL helpers and conveniences; implementation stays in `src/`. No vendor SDK symbols here.
 */

#ifndef HONKORDGL_GPUSHADERCOMPILER_H
#define HONKORDGL_GPUSHADERCOMPILER_H

#include <HonkordGL/Config.h>
#include <HonkordGL/GpuTypes.h>
#include <HonkordGL/WindowApplication.h>

#include <cstddef>
#include <string>

namespace HonkordGL::Graphics {

/** Target dialect for `GpuShaderPreamble` / `GpuShaderWrapBody`. */
enum class GpuShaderLanguage : unsigned char {
    /** Desktop shading profile (GLSL `#version 330 core`). */
    CoreProfile330,
    /**
     * Embedded / mobile-style fragment precision; vertex preamble minimal.
     */
    EmbeddedProfile2,
};

enum class GpuShaderStage : unsigned char {
    Vertex,
    Fragment,
};

HONKORDGL_API HONKORDGL_ND std::string GpuShaderPreamble(GpuShaderLanguage lang, GpuShaderStage stage);
HONKORDGL_API HONKORDGL_ND std::string GpuShaderWrapBody(
    GpuShaderLanguage lang,
    GpuShaderStage stage,
    const char * body);

/** Textured full-screen quad vertex shader (`layout(location=0)` pos, `layout(location=1)` uv). */
HONKORDGL_API HONKORDGL_ND std::string GpuShaderGeneratedTexturedQuadVertexCore330();

/** Textured quad fragment shader with flipped V; `sampler2D u_tex`, `out vec4 o_color`. */
HONKORDGL_API HONKORDGL_ND std::string GpuShaderGeneratedTexturedQuadFragmentCore330();

/** Solid fill fragment: `uniform vec4 u_color;` → `o_color`. */
HONKORDGL_API HONKORDGL_ND std::string GpuShaderGeneratedSolidColorFragmentCore330();

enum class GpuShaderCompileError : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    PROC_LOAD_FAILED = -3,
    VERTEX_COMPILE_FAILED = -4,
    FRAGMENT_COMPILE_FAILED = -5,
    LINK_FAILED = -6,
    /** `glCompileShader` failed for a shader object (see `infoLog`). */
    SHADER_COMPILE_FAILED = -7,
    /** Stage not supported on the active context (e.g. geometry on OpenGL ES 2). */
    STAGE_UNSUPPORTED = -8,
};

/**
 * Compile + link GLSL sources into a program object name.
 *
 * Requires a current graphics context on the calling thread (`GpuRenderer::MakeCurrent`, etc.).
 * If `ctx` is non-null, fails with `UNSUPPORTED_BACKEND` when the active backend cannot run user shaders here.
 *
 * @param outProgram receives a backend program name; set to 0 on failure.
 */
HONKORDGL_API HONKORDGL_ND int GpuShaderCompileProgram(
    ApplicationContextSettings const * ctx,
    const char * vertexSource,
    const char * fragmentSource,
    unsigned int * outProgram,
    char * infoLog,
    std::size_t infoLogCap,
    const int * attribLocations,
    const char * const * attribNames,
    int attribCount) noexcept;

/** Destroys a program object (no-op if `program` is 0). */
HONKORDGL_API void GpuShaderDeleteProgram(unsigned int program) noexcept;

/** Creates an empty shader object (`glCreateShader`). @param outShader set to 0 on failure. */
HONKORDGL_API HONKORDGL_ND int GpuShaderCreateObject(
    ApplicationContextSettings const * ctx,
    GpuShaderPipelineStage stage,
    unsigned int * outShader) noexcept;

/** Uploads GLSL source (`glShaderSource`). */
HONKORDGL_API HONKORDGL_ND int GpuShaderSetShaderSource(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    const char * source) noexcept;

/** Compiles a shader object (`glCompileShader`). */
HONKORDGL_API HONKORDGL_ND int GpuShaderCompileShaderObject(
    ApplicationContextSettings const * ctx,
    unsigned int shader,
    char * infoLog,
    std::size_t infoLogCap) noexcept;

/** Creates an empty program object (`glCreateProgram`). @param outProgram set to 0 on failure. */
HONKORDGL_API HONKORDGL_ND int GpuShaderCreateProgramObject(
    ApplicationContextSettings const * ctx,
    unsigned int * outProgram) noexcept;

/** Attaches a compiled shader to a program (`glAttachShader`). */
HONKORDGL_API void GpuShaderAttachShader(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int shader) noexcept;

/** Binds a vertex attribute name to a fixed location before link (`glBindAttribLocation`). */
HONKORDGL_API HONKORDGL_ND int GpuShaderBindAttribLocation(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    unsigned int location,
    const char * name) noexcept;

/** Links the program (`glLinkProgram`). */
HONKORDGL_API HONKORDGL_ND int GpuShaderLinkProgramObject(
    ApplicationContextSettings const * ctx,
    unsigned int program,
    char * infoLog,
    std::size_t infoLogCap) noexcept;

/** Deletes a shader object (`glDeleteShader`, no-op if `shader` is 0). */
HONKORDGL_API void GpuShaderDeleteShaderObject(unsigned int shader) noexcept;

/**
 * Resolves compiler entry points on platforms that load them at runtime. Idempotent.
 * @return `GpuShaderCompileError` as int (`OK` or `PROC_LOAD_FAILED`).
 */
HONKORDGL_API HONKORDGL_ND int GpuShaderLoadCompilerProcs() noexcept;

} // namespace HonkordGL::Graphics

#endif
