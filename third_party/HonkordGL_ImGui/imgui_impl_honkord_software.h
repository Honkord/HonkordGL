/**
 * Dear ImGui + HonkordGL **CPU software framebuffer** (`SoftwareRenderer` only — no GPU attach).
 */

#pragma once

#include "imgui.h"

#ifndef IMGUI_DISABLE

#include <HonkordGL/Events.h>
#include <cstdint>

namespace HonkordGL::Graphics {
struct ApplicationContextSettings;
class SoftwareRenderer;
}

IMGUI_IMPL_API bool ImGui_ImplHonkordSoftware_Init(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    HonkordGL::Graphics::SoftwareRenderer * renderer);

IMGUI_IMPL_API void ImGui_ImplHonkordSoftware_Shutdown();

IMGUI_IMPL_API void ImGui_ImplHonkordSoftware_NewFrame(HonkordGL::Graphics::ApplicationContextSettings * app);

IMGUI_IMPL_API bool ImGui_ImplHonkordSoftware_ProcessEvent(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    HonkordGL::Events::Event const & ev);

IMGUI_IMPL_API void ImGui_ImplHonkordSoftware_UpdateTexture(ImTextureData * tex);

/** Rasterizes ImGui into `renderer->Surface()` and leaves it ready for `SoftwareRenderer::Present()` (CPU blit). */
IMGUI_IMPL_API void ImGui_ImplHonkordSoftware_RenderDrawData(
    ImDrawData * draw_data,
    HonkordGL::Graphics::SoftwareRenderer * renderer,
    std::uint8_t clear_r = 28,
    std::uint8_t clear_g = 30,
    std::uint8_t clear_b = 38);

#endif
