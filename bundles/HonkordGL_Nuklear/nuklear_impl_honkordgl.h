/**
 * Nuklear + HonkordGL `GpuRenderer` (OpenGL family) backend.
 */

#pragma once

#include "nuklear_honkord_defs.h"
#include "nuklear.h"

#include <HonkordGL/Events.h>

namespace HonkordGL::Graphics {
struct ApplicationContextSettings;
class GpuRenderer;
}

/** Creates font atlas + GL texture, initializes `ctx` with `nk_init_default`. */
bool Nuklear_ImplHonkordGL_Init(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    HonkordGL::Graphics::GpuRenderer * gpu,
    struct nk_context * ctx,
    float font_height_pixels = 13.f);

void Nuklear_ImplHonkordGL_Shutdown(struct nk_context * ctx);

/** Uploads `nk_convert` geometry and draws with blending + scissor; disables scissor after (like ImGui backend). */
void Nuklear_ImplHonkordGL_Render(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    struct nk_context * ctx,
    HonkordGL::Graphics::GpuRenderer * gpu);

bool Nuklear_ImplHonkordGL_CreateDeviceObjects(HonkordGL::Graphics::GpuRenderer * gpu);
void Nuklear_ImplHonkordGL_DestroyDeviceObjects(HonkordGL::Graphics::GpuRenderer * gpu);
