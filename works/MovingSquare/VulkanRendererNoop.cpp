/**
 * Satisfies GLX `Internal::*Vulkan` symbols when not linking `src/VulkanRendererContext.cpp`
 * (e.g. no Vulkan headers). OpenGL-only samples do not use these at runtime.
 */
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics::Internal {

int AttachRendererContextVulkan(ApplicationContextSettings &, RendererContextSettings const &) noexcept
{
    return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
}

void DetachRendererContextVulkan(ApplicationContextSettings & app) noexcept
{
    app.renderer_private = nullptr;
    app.active_renderer = 0;
}

int RendererContextMakeCurrentVulkan(ApplicationContextSettings &) noexcept
{
    return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
}

void RendererContextSwapBuffersVulkan(ApplicationContextSettings &) noexcept {}

} // namespace HonkordGL::Graphics::Internal