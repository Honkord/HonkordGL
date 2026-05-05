/**
 * HonkordGL — Direct3D 11 stubs when `HONKORDGL_ENABLE_D3D11=OFF`
 * Copyright (C) 2026 Honkord
 */

#include "../internal/D3D11RendererContextWin32.hpp"

#if defined(_WIN32)

#include <HonkordGL/Direct3DIntegration.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/Video.h>

#include <cstring>

namespace HonkordGL::Graphics::Internal {

D3D11HonkordRendererState * GetD3D11State(ApplicationContextSettings & app) noexcept
{
    (void)app;
    return nullptr;
}

int AttachD3D11(ApplicationContextSettings &, RendererContextSettings const &) noexcept
{
    SetInternalApiError("AttachRendererContext (D3D11): Direct3D 11 was disabled at configure time (HONKORDGL_ENABLE_D3D11=OFF).");
    return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
}

void DetachD3D11(ApplicationContextSettings & app) noexcept
{
    app.renderer_private = nullptr;
    app.device = nullptr;
    app.device_context = nullptr;
    app.surface = nullptr;
    app.active_renderer = 0;
}

int RendererContextMakeCurrentD3D11(ApplicationContextSettings &) noexcept
{
    return static_cast<int>(RendererContextResult::UNSUPPORTED_BACKEND);
}

void RendererContextSwapBuffersD3D11(ApplicationContextSettings &) noexcept
{
}

int D3D11CreateTexture2DRgba8(
    ApplicationContextSettings &,
    int,
    int,
    void const *,
    GpuObjectName * outTexture) noexcept
{
    if (outTexture)
        *outTexture = 0;
    return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
}

int D3D11UpdateTexture2DRgba8(
    ApplicationContextSettings &,
    GpuObjectName,
    int,
    int,
    void const *) noexcept
{
    return static_cast<int>(GpuShaderCompileError::UNSUPPORTED_BACKEND);
}

void D3D11BindTexture2D(ApplicationContextSettings &, unsigned, GpuObjectName) noexcept
{
}

void D3D11DestroyTexture(ApplicationContextSettings &, GpuObjectName) noexcept
{
}

} // namespace HonkordGL::Graphics::Internal

namespace HonkordGL::Graphics {

int Direct3D11IntegrationGetNativeHandles(ApplicationContextSettings const *, Direct3D11NativeHandles *) noexcept
{
    return static_cast<int>(Direct3DIntegrationResult::UNSUPPORTED_BACKEND);
}

} // namespace HonkordGL::Graphics

#endif // defined(_WIN32)
