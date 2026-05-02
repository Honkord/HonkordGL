/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Public deferred renderer sample wrappers.
 */

#include <HonkordGL/DeferredRendererSample.h>

#include "internal/DeferredRendererGpuApi.h"

namespace HonkordGL::Graphics
{

int DeferredRendererSampleCreate(
    ApplicationContextSettings & app,
    WindowBackend & backend,
    DeferredRendererSampleSettings const & settings,
    DeferredRendererSampleContext * * outContext) noexcept
{
    return Internal::CreateDeferredRendererGpuApi(app, backend, settings, outContext);
}

void RunDeferredRendererSampleMain(
    DeferredRendererSampleContext * context,
    DeferredRendererSampleSettings const & settings) noexcept
{
    Internal::RunDeferredRendererGpuApiFrame(context, settings);
}

void DeferredRendererSampleDestroy(DeferredRendererSampleContext * context) noexcept
{
    Internal::DestroyDeferredRendererGpuApi(context);
}

} // namespace HonkordGL::Graphics
