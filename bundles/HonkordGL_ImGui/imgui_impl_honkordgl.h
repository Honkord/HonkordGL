/**
 * Dear ImGui renderer + HonkordGL window/input bridge.
 * Requires an OpenGL-family `GpuRenderer` attach (`Renderers::OPENGL` / `EGL`).
 */

#pragma once

#include "imgui.h"

#ifndef IMGUI_DISABLE

#include <HonkordGL/Events.h>

namespace HonkordGL::Graphics {
struct ApplicationContextSettings;
class GpuRenderer;
}

/**
 * Register renderer backend and allocate GPU objects (shaders, buffers).
 * Call after `GpuRenderer::Create*` succeeds on `gpu` bound to `app`.
 */
IMGUI_IMPL_API bool ImGui_ImplHonkordGL_Init(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    HonkordGL::Graphics::GpuRenderer * gpu);

IMGUI_IMPL_API void ImGui_ImplHonkordGL_Shutdown();

/** Sets display size, framebuffer scale, and `io.DeltaTime` — call once per frame before `ImGui::NewFrame()`. */
IMGUI_IMPL_API void ImGui_ImplHonkordGL_NewFrame(HonkordGL::Graphics::ApplicationContextSettings * app);

/** Forward HonkordGL events after `PollEvent`, before `ImGui::NewFrame()`. */
IMGUI_IMPL_API bool ImGui_ImplHonkordGL_ProcessEvent(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    HonkordGL::Events::Event const & ev);

/** Font atlas / texture uploads (`ImGuiBackendFlags_RendererHasTextures`). Called internally from `RenderDrawData`. */
IMGUI_IMPL_API void ImGui_ImplHonkordGL_UpdateTexture(ImTextureData * tex);

IMGUI_IMPL_API void ImGui_ImplHonkordGL_RenderDrawData(
    ImDrawData * draw_data,
    HonkordGL::Graphics::GpuRenderer * gpu);

IMGUI_IMPL_API bool ImGui_ImplHonkordGL_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplHonkordGL_DestroyDeviceObjects();

#endif // IMGUI_DISABLE
