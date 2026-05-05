/**
 * HonkordGL — Vulkan window / WSI helpers (no Vulkan API types in this TU’s public symbols)
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/PlatformAdapter.h>
#include <HonkordGL/RendererContext.h>
#include <HonkordGL/VulkanIntegration.h>
#include <HonkordGL/WindowApplication.h>

#include <cstring>

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
# include "Wayland/WaylandWindowContext.hpp"
# include <X11/Xlib.h>
#endif

namespace HonkordGL::Graphics {

void VulkanIntegrationRecommendAttachHints(RendererContextSettings * spec) noexcept
{
    if (!spec)
        return;
    RendererContextSettings s{};
    s.backend = Renderers::VULKAN;
    s.gl_major = 0;
    s.gl_minor = 0;
    s.gl_core_profile = 0;
    s.gl_forward_compatible = 0;
    s.double_buffer = 1;
    s.color_bits_override = 0;
    s.depth_bits_override = 0;
    s.stencil_bits_override = 0;
    s.msaa_samples_override = 0;
    s.vsync = 1;
    s.minimum_honkord_gl_api_level = 0;
    s.requested_gpu_device = 0;
    *spec = s;
}

int VulkanIntegrationQueryWindowSurfaceReadiness(
    ApplicationContextSettings const * app,
    VulkanWindowSurfaceInfo * out) noexcept
{
    if (!app || !out)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    std::memset(out, 0, sizeof(*out));

#if HONKORDGL_PLATFORM_LINUX_DESKTOP
    const auto plat = static_cast<NativePlatform>(app->native_platform);
    if (plat == NativePlatform::Wayland) {
        out->surface_kind = VulkanWindowSurfaceKind::Wayland;
        out->needs_instance_extension_khr_surface = 1;
        out->needs_instance_extension_khr_wayland_surface = 1;
        auto * const wctx = reinterpret_cast<Wayland::Internal::WaylandWindowContext *>(app->window_handle);
        out->window_ready_for_vulkan_surface = (wctx && wctx->display && wctx->surface) ? 1 : 0;
        return static_cast<int>(VulkanIntegrationResult::OK);
    }
    if (NativePlatformUsesX11ClientHandles(plat)) {
        out->surface_kind = VulkanWindowSurfaceKind::Xlib;
        out->needs_instance_extension_khr_surface = 1;
        out->needs_instance_extension_khr_xlib_surface = 1;
        auto * const dpy = reinterpret_cast<Display *>(app->device_context);
        const ::Window win = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(app->window_handle));
        out->window_ready_for_vulkan_surface = (dpy && win) ? 1 : 0;
        return static_cast<int>(VulkanIntegrationResult::OK);
    }
#endif
    (void)app;
    out->surface_kind = VulkanWindowSurfaceKind::NoSurface;
    out->window_ready_for_vulkan_surface = 0;
    return static_cast<int>(VulkanIntegrationResult::OK);
}

namespace {

char const kKHRSurface[] = "VK_KHR_surface";
char const kKHRXlibSurface[] = "VK_KHR_xlib_surface";
char const kKHRWaylandSurface[] = "VK_KHR_wayland_surface";

} // namespace

int VulkanIntegrationGetRecommendedInstanceExtensionNamesForWindow(
    ApplicationContextSettings const * app,
    char const ** out_names,
    int max_names,
    int * out_count) noexcept
{
    if (!app || !out_count)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);

    VulkanWindowSurfaceInfo info{};
    const int q = VulkanIntegrationQueryWindowSurfaceReadiness(app, &info);
    if (q != static_cast<int>(VulkanIntegrationResult::OK))
        return q;

    int needed = 0;
    if (info.needs_instance_extension_khr_surface)
        ++needed;
    if (info.needs_instance_extension_khr_xlib_surface)
        ++needed;
    if (info.needs_instance_extension_khr_wayland_surface)
        ++needed;

    if (needed == 0) {
        *out_count = 0;
        return static_cast<int>(VulkanIntegrationResult::OK);
    }
    if (max_names < needed || !out_names) {
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    }

    int n = 0;
    if (info.needs_instance_extension_khr_surface)
        out_names[n++] = kKHRSurface;
    if (info.needs_instance_extension_khr_xlib_surface)
        out_names[n++] = kKHRXlibSurface;
    if (info.needs_instance_extension_khr_wayland_surface)
        out_names[n++] = kKHRWaylandSurface;

    *out_count = n;
    return static_cast<int>(VulkanIntegrationResult::OK);
}

} // namespace HonkordGL::Graphics
