/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Direct3D 11 escape hatch (handles as opaque pointers; include `<d3d11.h>` to cast)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_DIRECT3DINTEGRATION_H
#define HONKORDGL_DIRECT3DINTEGRATION_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

namespace HonkordGL::Graphics {

enum class Direct3DIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    UNSUPPORTED_PLATFORM = -3,
};

struct Direct3D11NativeHandles {
    HonkordGL_GW_Handle dxgi_factory{};
    HonkordGL_GW_Handle d3d11_device{};
    HonkordGL_GW_Handle d3d11_immediate_context{};
    HonkordGL_GW_Handle dxgi_swap_chain{};
    HonkordGL_GW_Handle d3d11_render_target_view{};
    unsigned int backbuffer_width{};
    unsigned int backbuffer_height{};
};

HONKORDGL_API HONKORDGL_ND int Direct3D11IntegrationGetNativeHandles(
    ApplicationContextSettings const * app,
    Direct3D11NativeHandles * out) noexcept;

} // namespace HonkordGL::Graphics

#endif
