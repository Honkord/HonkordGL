/**
    * @author Honkord <https://github.com>
    *
    * HonkordGL — attach a rendering API to an existing application context
    * Copyright (C) 2026 Honkord
    */

#ifndef HONKORDGL_RENDERERCONTEXT_H
#define HONKORDGL_RENDERERCONTEXT_H

#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics {

/** Result of renderer-context operations (0 = success). */
enum class RendererContextResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    UNSUPPORTED_PLATFORM = -3,
    NO_DISPLAY = -4,
    NO_WINDOW = -5,
    PIXEL_FORMAT_FAILED = -6,
    CONTEXT_CREATE_FAILED = -7,
    MAKE_CURRENT_FAILED = -8,
    NO_RENDERER_ATTACHED = -9,
    ALREADY_ATTACHED = -10,
};

/**
 * Describes which `Renderers` backend to attach and optional version / surface hints.
 * `WindowBackend::CreateWindow` must have filled `ApplicationContextSettings` before `AttachRendererContext`.
 * Native handle layout in `ApplicationContextSettings` is defined by the active platform driver in this library.
 */
HONKORDGL_API struct RendererContextSettings {
    Renderers backend;

    /** Raster API major/minor request; `0`/`0` selects implementation defaults where applicable. */
    int gl_major;
    int gl_minor;
    int gl_core_profile;        /**< 1 = request core profile */
    int gl_forward_compatible; /**< 1 = forward-compatible context */

    int double_buffer; /**< 1 = double-buffered (recommended) */

    /** 0 = take from ApplicationContextSettings (color_bits / depth_bits / stencil_bits / msaa_samples) */
    int color_bits_override;
    int depth_bits_override;
    int stencil_bits_override;
    int msaa_samples_override;

    /** 1 = request adaptive vsync via GLX swap interval when the extension is present */
    int vsync;

    RendererContextSettings() noexcept
        : backend(Renderers::OPENGL),
          gl_major(3),
          gl_minor(3),
          gl_core_profile(1),
          gl_forward_compatible(1),
          double_buffer(1),
          color_bits_override(0),
          depth_bits_override(0),
          stencil_bits_override(0),
          msaa_samples_override(0),
          vsync(1)
    {
    }
};

/**
 * Creates a native rendering context for `spec.backend` and binds it into `app`.
 * If a context is already stored in `app.device`, returns ALREADY_ATTACHED (call DetachRendererContext first).
 *
 * @see GpuRenderer — helper bound to `ApplicationContextSettings`; `CreateBestAvailable()` probes backends, `CreateBestAvailableShaderBackend()` attaches a raster path suitable for user shader code.
 * @see GpuShaderCompiler.h — shader source helpers and `GpuShaderCompileProgram`.
 * @see PlatformAdapter.h — compile-time platform macros and `HonkordGL::BuildPlatform` (included via `Config.h`).
 */
HONKORDGL_API int AttachRendererContext(
    ApplicationContextSettings& app,
    const RendererContextSettings& spec) noexcept;

/** Destroys the attached context and clears `app.device` (and related fields where applicable). */
HONKORDGL_API void DetachRendererContext(ApplicationContextSettings& app) noexcept;

/** Makes the attached graphics context current for this thread. */
HONKORDGL_API int RendererContextMakeCurrent(ApplicationContextSettings& app) noexcept;

/** Presents the current frame to the window / surface. */
HONKORDGL_API void RendererContextSwapBuffers(ApplicationContextSettings& app) noexcept;

} // namespace HonkordGL::Graphics

#endif