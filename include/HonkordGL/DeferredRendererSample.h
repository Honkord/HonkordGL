/**
 * @author Honkord <https://github.com>
 *
 * Copyright (C) 2026 Honkord
 *
 * Deferred rendering sample: you create the window (`WindowBackend::CreateWindow`), then
 * `DeferredRendererSampleCreate`, then call `RunDeferredRendererSampleMain(context, settings)` once per frame
 * from your event loop (after `Events::PollEvent`). Release with `DeferredRendererSampleDestroy`.
 *
 * Implementation: `src/DeferredRendererSample.cpp`, `src/internal/DeferredRendererGpuApi.cpp`.
 */

#pragma once

#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>

namespace HonkordGL::Graphics
{

/** Opaque state for the built-in deferred sample (G-buffer, programs, meshes). */
struct DeferredRendererSampleContext;

/** User-tunable options (visuals; window placement is applied by the app before `CreateWindow`). */
struct DeferredRendererSampleSettings
{
    /** Window title; if null, use a default string when filling `ApplicationContextSettings`. */
    const char * window_title = nullptr;

    int client_x = 80;
    int client_y = 80;
    int client_w = 960;
    int client_h = 600;

    /** Passed to `WindowBackend::SetTargetFrameRate` during `DeferredRendererSampleCreate`; 0 → 60 Hz. */
    unsigned target_fps = 60;

    float mesh_albedo[3] = { 0.85f, 0.45f, 0.25f };
    float light_dir_world[3] = { 0.55f, 0.75f, 0.35f };
    float light_color[3] = { 1.f, 0.97f, 0.9f };
    float background_rgb[3] = { 0.02f, 0.025f, 0.04f };
};

/**
 * After `WindowBackend::CreateWindow(ctx)`, builds shaders, meshes, and G-buffer for the sample.
 * On success, `*outContext` is non-null; caller must eventually call `DeferredRendererSampleDestroy`.
 * @return 0 on success, non-zero on failure (context is not allocated).
 */
HONKORDGL_ND int DeferredRendererSampleCreate(
    ApplicationContextSettings & app,
    WindowBackend & backend,
    DeferredRendererSampleSettings const & settings,
    DeferredRendererSampleContext * * outContext) noexcept;

/**
 * Renders one frame (geometry → G-buffer → lighting → swap). Call only from inside your main window loop,
 * after draining input events for the frame (e.g. `Events::PollEvent`).
 */
void RunDeferredRendererSampleMain(
    DeferredRendererSampleContext * context,
    DeferredRendererSampleSettings const & settings) noexcept;

/** Releases all sample GPU resources; does not destroy the window. */
void DeferredRendererSampleDestroy(DeferredRendererSampleContext * context) noexcept;

} // namespace HonkordGL::Graphics
