/**
 * HonkordGL — internal deferred renderer implementation.
 * Not part of the public API.
 */

#pragma once

#include <HonkordGL/DeferredRendererSample.h>

namespace HonkordGL::Graphics::Internal
{
int CreateDeferredRendererGpuApi(
    ApplicationContextSettings & app,
    WindowBackend & backend,
    DeferredRendererSampleSettings const & settings,
    DeferredRendererSampleContext ** outContext) noexcept;

void RunDeferredRendererGpuApiFrame(
    DeferredRendererSampleContext * context,
    DeferredRendererSampleSettings const & settings) noexcept;

void DestroyDeferredRendererGpuApi(DeferredRendererSampleContext * context) noexcept;

} // namespace HonkordGL::Graphics::Internal