/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — OpenGL integration helpers (native handles + hints; no vendor headers here)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_OPENGLINTEGRATION_H
#define HONKORDGL_OPENGLINTEGRATION_H

#include <HonkordGL/Config.h>
#include <HonkordGL/RendererContext.h>

#include <cstdint>

namespace HonkordGL::Graphics {

/** Raster family relevant to OpenGL-style integration. */
enum class OpenGlIntegrationApiKind : unsigned char {
    None = 0,
    /** GLX / WGL / NSOpenGL desktop GL. */
    DesktopGl = 1,
    /** EGL-managed GL (X11/Wayland EGL, etc.). */
    EglGl = 2,
};

/** Stable result codes for OpenGlIntegration* entry points. */
enum class OpenGlIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    MAKE_CURRENT_FAILED = -8,
    NO_CONTEXT = -9,
};

/**
 * Runtime / surface hints for binding a third-party OpenGL loader or GL-based engine to HonkordGL.
 * Integer fields mirror `ApplicationContextSettings` where applicable; GL version fields are best-effort
 * (queried after a successful `RendererContextMakeCurrent` inside `OpenGlIntegrationQueryHints`).
 */
struct OpenGlIntegrationHints {
    OpenGlIntegrationApiKind api_kind{OpenGlIntegrationApiKind::None};
    int gl_major_version{};
    int gl_minor_version{};
    int effective_color_bits{};
    int effective_depth_bits{};
    int effective_stencil_bits{};
    int effective_msaa_samples{};
    int double_buffered{};
    int vsync_enabled{};
    int requested_core_profile{};
    int requested_forward_compatible{};
    int framebuffer_width{};
    int framebuffer_height{};
};

/**
 * Opaque native slots HonkordGL filled during `AttachRendererContext` for OpenGL / EGL GL paths.
 * Interpretation by `NativePlatform` / `Renderers` (see `ApplicationContextSettings` comments):
 * - **GLX**: `rendering_context` = `GLXContext`, `platform_display_or_connection` = `Display*`,
 *   `drawable_surface` = `GLXDrawable` (typically the X window), `egl_display_if_any` = null.
 * - **WGL**: `rendering_context` = `HGLRC`, `drawable_surface` = `HDC` used for `wglMakeCurrent`,
 *   `window_host` = underlying `HWND` (via `window_handle`), `platform_display_or_connection` often null.
 * - **EGL (X11/Wayland)**: `rendering_context` = `EGLContext`, `egl_display_if_any` = `EGLDisplay`,
 *   `drawable_surface` = `EGLSurface`, `platform_display_or_connection` = native display if applicable.
 * - **macOS NSOpenGL**: `rendering_context` = `NSOpenGLContext*`, `window_host` = `NSWindow*` (opaque).
 */
struct OpenGlNativeRenderingHandles {
    HonkordGL_GW_Handle rendering_context{nullptr};
    HonkordGL_GW_Handle platform_display_or_connection{nullptr};
    HonkordGL_GW_Handle drawable_surface{nullptr};
    HonkordGL_GW_Handle egl_display_if_any{nullptr};
    HonkordGL_GW_Handle window_host{nullptr};
};

/** @return true when `Renderers::OPENGL` or `Renderers::EGL` is active on `app`. */
HONKORDGL_API HONKORDGL_ND bool OpenGlIntegrationIsActive(ApplicationContextSettings const * app) noexcept;

/** Copies native slots from `app` into `out` (zeroed when `out` or `app` is null). */
HONKORDGL_API void OpenGlIntegrationGetNativeRenderingHandles(
    ApplicationContextSettings const * app,
    OpenGlNativeRenderingHandles * out) noexcept;

/**
 * Makes the context current, then fills pixel-format fields from `app` and GL version fields when queries exist.
 * @return `OpenGlIntegrationResult` as int.
 */
HONKORDGL_API HONKORDGL_ND int OpenGlIntegrationQueryHints(
    ApplicationContextSettings * app,
    OpenGlIntegrationHints * out_hints) noexcept;

/**
 * Fills `spec` with conservative defaults for integrating a desktop GL stack (`Renderers::OPENGL`)
 * or EGL GL (`Renderers::EGL`). Safe to call before `AttachRendererContext`; does not attach.
 */
HONKORDGL_API void OpenGlIntegrationRecommendAttachHints(
    RendererContextSettings * spec,
    Renderers backend,
    int gl_major,
    int gl_minor,
    int core_profile,
    int forward_compatible,
    int double_buffer,
    int vsync) noexcept;

} // namespace HonkordGL::Graphics

#endif
