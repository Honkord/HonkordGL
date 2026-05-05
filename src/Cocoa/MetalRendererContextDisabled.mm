/**
 * HonkordGL — Metal RendererContext stubs when `HONKORDGL_ENABLE_METAL=OFF`
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/MetalIntegration.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>

#if defined(__APPLE__)

#include <cstring>

namespace HonkordGL::Graphics::Internal {

int AttachRendererContextMetal(ApplicationContextSettings &, RendererContextSettings const &) noexcept
{
    SetInternalApiError("AttachRendererContext (Metal): Metal was disabled at configure time (HONKORDGL_ENABLE_METAL=OFF).");
    return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
}

void DetachRendererContextMetal(ApplicationContextSettings & app) noexcept
{
    app.renderer_private = nullptr;
    app.device = nullptr;
    app.surface = nullptr;
    app.active_renderer = 0;
}

int RendererContextMakeCurrentMetal(ApplicationContextSettings &) noexcept
{
    return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
}

void RendererContextSwapBuffersMetal(ApplicationContextSettings &) noexcept
{
}

int MetalIntegrationFillHandles(ApplicationContextSettings const *, MetalNativeHandles *) noexcept
{
    return static_cast<int>(MetalIntegrationResult::UNSUPPORTED_BACKEND);
}

} // namespace HonkordGL::Graphics::Internal

namespace HonkordGL::Graphics {

int MetalIntegrationGetNativeHandles(ApplicationContextSettings const * app, MetalNativeHandles * out) noexcept
{
    return Internal::MetalIntegrationFillHandles(app, out);
}

} // namespace HonkordGL::Graphics

#endif // defined(__APPLE__)
