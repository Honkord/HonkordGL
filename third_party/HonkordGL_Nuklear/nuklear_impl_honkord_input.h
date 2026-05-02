/**
 * HonkordGL events → Nuklear input (shared by GPU and CPU backends).
 */

#pragma once

#include "nuklear_honkord_defs.h"
#include "nuklear.h"

#include <HonkordGL/Events.h>

namespace HonkordGL::Graphics {
struct ApplicationContextSettings;
}

void Nuklear_ImplHonkord_InputBegin(struct nk_context * ctx) noexcept;

void Nuklear_ImplHonkord_InputEnd(struct nk_context * ctx) noexcept;

/** Feed after `InputBegin`, typically inside your `PollEvent` loop. */
void Nuklear_ImplHonkord_ProcessEvent(
    HonkordGL::Graphics::ApplicationContextSettings * app,
    struct nk_context * ctx,
    HonkordGL::Events::Event const & ev) noexcept;
