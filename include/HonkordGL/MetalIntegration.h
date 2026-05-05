/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Metal escape hatch (Objective-C objects bridged as opaque `HonkordGL_GW_Handle`; cast in .mm with ARC rules)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_METALINTEGRATION_H
#define HONKORDGL_METALINTEGRATION_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics {

enum class MetalIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    UNSUPPORTED_PLATFORM = -3,
};

struct MetalNativeHandles {
    HonkordGL_GW_Handle mtl_device{};
    HonkordGL_GW_Handle mtl_command_queue{};
    HonkordGL_GW_Handle ca_metal_layer{};
};

HONKORDGL_API HONKORDGL_ND int MetalIntegrationGetNativeHandles(
    ApplicationContextSettings const * app,
    MetalNativeHandles * out) noexcept;

} // namespace HonkordGL::Graphics

#endif
