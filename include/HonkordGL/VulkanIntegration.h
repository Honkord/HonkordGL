/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Vulkan escape hatch (vendor-neutral declarations; cast handles after `#include <vulkan/vulkan.h>`)
 * Copyright (C) 2026 Honkord
 */

#ifndef HONKORDGL_VULKANINTEGRATION_H
#define HONKORDGL_VULKANINTEGRATION_H

#include <HonkordGL/Config.h>
#include <HonkordGL/WindowApplication.h>

#include <cstddef>
#include <cstdint>

namespace HonkordGL::Graphics {

/** Stable result codes for VulkanIntegration* entry points. */
enum class VulkanIntegrationResult : int {
    OK = 0,
    INVALID_ARGUMENT = -1,
    UNSUPPORTED_BACKEND = -2,
    UNSUPPORTED_PLATFORM = -3,
};

/**
 * Opaque slots matching the active Vulkan attach (`Renderers::VULKAN`). Cast to Vulkan types in user code
 * after including `<vulkan/vulkan.h>` (dispatchable handles are pointer-sized).
 */
struct VulkanIntegrationNativeHandles {
    HonkordGL_GW_Handle instance{};
    HonkordGL_GW_Handle physical_device{};
    HonkordGL_GW_Handle device{};
    HonkordGL_GW_Handle surface{};
    HonkordGL_GW_Handle swapchain{};
    HonkordGL_GW_Handle graphics_queue{};
    HonkordGL_GW_Handle render_pass{};
    HonkordGL_GW_Handle command_pool{};
    HonkordGL_GW_Handle command_buffer{};
    HonkordGL_GW_Handle image_available_semaphore{};
    HonkordGL_GW_Handle render_finished_semaphore{};
    HonkordGL_GW_Handle flight_fence{};
    std::uint32_t graphics_queue_family_index{0};
    std::uint32_t present_queue_family_index{0};
    std::uint32_t swapchain_image_count{0};
    /** Up to 16 swapchain slot pairs (image handle is non-dispatchable uint64-sized on many targets — still stored as handle-sized). */
    HonkordGL_GW_Handle swapchain_images[16]{};
    HonkordGL_GW_Handle swapchain_image_views[16]{};
    HonkordGL_GW_Handle swapchain_framebuffers[16]{};
    std::uint32_t swapchain_width{};
    std::uint32_t swapchain_height{};
};

/** @return `VulkanIntegrationResult` as int when `app` has an attached Vulkan backend and optional fields are copied. */
HONKORDGL_API HONKORDGL_ND int VulkanIntegrationGetNativeHandles(
    ApplicationContextSettings const * app,
    VulkanIntegrationNativeHandles * out) noexcept;

/** Loader entry points (`instance` may be null for global symbols where supported). */
HONKORDGL_API HONKORDGL_ND void * VulkanIntegrationGetInstanceProcAddr(
    HonkordGL_GW_Handle instance,
    char const * name) noexcept;

HONKORDGL_API HONKORDGL_ND void * VulkanIntegrationGetDeviceProcAddr(
    HonkordGL_GW_Handle device,
    char const * name) noexcept;

/** Returns `vkGetInstanceProcAddr` as a function pointer (same signature as Vulkan loader). */
HONKORDGL_API HONKORDGL_ND void * VulkanIntegrationGetVulkanGetInstanceProcAddr() noexcept;

/**
 * Enumerates instance extensions (layers optional). Invokes `callback` once per extension (may be called with null
 * args if `callback` is null — then returns count only when `out_count` non-null).
 * @return Number of extensions enumerated on success; negative `VulkanIntegrationResult` on failure.
 */
using VulkanIntegrationExtensionCallback = void (*)(void * user, char const * extension_name, std::uint32_t spec_version);

HONKORDGL_API HONKORDGL_ND int VulkanIntegrationEnumerateInstanceExtensions(
    char const * layer_name_or_null,
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept;

HONKORDGL_API HONKORDGL_ND int VulkanIntegrationEnumerateInstanceLayers(
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept;

/**
 * Enumerates device extensions for `physical_device` opaque handle (pass Vulkan `VkPhysicalDevice` cast through handle).
 */
HONKORDGL_API HONKORDGL_ND int VulkanIntegrationEnumerateDeviceExtensions(
    HonkordGL_GW_Handle physical_device,
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept;

} // namespace HonkordGL::Graphics

#endif
