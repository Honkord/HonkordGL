/**
 * HonkordGL — backend availability queries (implementation)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/BackendCapabilities.h>

#include <HonkordGL/BuildFeatures.h>

namespace HonkordGL::Graphics {

bool IsOpenGLBackendAvailable() noexcept
{
#if HONKORDGL_HAVE_OPENGL
    return true;
#else
    return false;
#endif
}

bool IsVulkanBackendAvailable() noexcept
{
#if HONKORDGL_HAVE_VULKAN
    return true;
#else
    return false;
#endif
}

bool IsDirect3D11BackendAvailable() noexcept
{
#if HONKORDGL_HAVE_D3D11
    return true;
#else
    return false;
#endif
}

bool IsMetalBackendAvailable() noexcept
{
#if HONKORDGL_HAVE_METAL
    return true;
#else
    return false;
#endif
}

bool IsAudioBackendAvailable() noexcept
{
#if HONKORDGL_HAVE_AUDIO
    return true;
#else
    return false;
#endif
}

} // namespace HonkordGL::Graphics
