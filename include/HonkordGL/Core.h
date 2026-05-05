/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Core public surface (windowing, events, GPU attach, limits, interop)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_CORE_H
#define HONKORDGL_CORE_H

/**
 * @file Core.h
 * @brief Minimal **stable** include set for engine-style use: platform config, window/session,
 *        framebuffer-oriented `Video` API, renderer attach / swap, GPU limits, OpenGL interop handles,
 *        and shader compilation helpers. No sprites, software raster stack, or IPC helpers.
 *
 * Prefer `#include <HonkordGL/Core.h>` when you want only this layer; use `HonkordGL.h` or
 * `Extras.h` for optional higher-level modules (audio, joystick, `SoftwareRenderer`, game helpers).
 */

#include <HonkordGL/Events.h>
#include <HonkordGL/WindowApplication.h>
#include <HonkordGL/WindowBackend.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/GpuApiBoundary.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/GpuCapabilities.h>
#include <HonkordGL/BackendCapabilities.h>
#include <HonkordGL/GpuRenderer.h>
#include <HonkordGL/GpuRenderTarget.h>
#include <HonkordGL/GpuRenderPassEncoder.h>
#include <HonkordGL/GpuGlStateTracker.h>
#include <HonkordGL/GpuRenderGraph.h>
#include <HonkordGL/OpenGlIntegration.h>
#include <HonkordGL/GpuShaderCompiler.h>
#include <HonkordGL/VulkanIntegration.h>
#include <HonkordGL/Direct3DIntegration.h>
#include <HonkordGL/MetalIntegration.h>
#include <HonkordGL/LinuxDisplayIntegration.h>
#include <HonkordGL/LinuxEvdevIntegration.h>

#endif
