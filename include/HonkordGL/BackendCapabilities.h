/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — compile-time / link-time backend availability queries (no vendor SDK types)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_BACKEND_CAPABILITIES_H
#define HONKORDGL_BACKEND_CAPABILITIES_H

#include <HonkordGL/Config.h>

namespace HonkordGL::Graphics {

/** True when this build includes the OpenGL family renderer path (desktop GL, GLES, WGL, GLX, EGL, NSOpenGL as applicable). */
HONKORDGL_API bool IsOpenGLBackendAvailable() noexcept;

/** True when this build links a Vulkan renderer context (integration API may still be stubbed on some targets). */
HONKORDGL_API bool IsVulkanBackendAvailable() noexcept;

/** True when Direct3D 11 attach paths are compiled in (Windows). */
HONKORDGL_API bool IsDirect3D11BackendAvailable() noexcept;

/** True when Metal attach paths are compiled in (Apple). */
HONKORDGL_API bool IsMetalBackendAvailable() noexcept;

/** True when the audio subsystem implementation is compiled in (not the disabled-audio stub). */
HONKORDGL_API bool IsAudioBackendAvailable() noexcept;

} // namespace HonkordGL::Graphics

#endif
