/**
 * HonkordGL — VulkanIntegration entry points when full `VulkanRendererContext.cpp` Linux path is not linked
 * (Windows / stub builds).
 * Copyright (C) 2026 Honkord
 */

#include <HonkordGL/VulkanIntegration.h>

namespace HonkordGL::Graphics {

int VulkanIntegrationGetNativeHandles(ApplicationContextSettings const *, VulkanIntegrationNativeHandles *) noexcept
{
    return static_cast<int>(VulkanIntegrationResult::UNSUPPORTED_PLATFORM);
}

void * VulkanIntegrationGetInstanceProcAddr(HonkordGL_GW_Handle, char const *) noexcept
{
    return nullptr;
}

void * VulkanIntegrationGetDeviceProcAddr(HonkordGL_GW_Handle, char const *) noexcept
{
    return nullptr;
}

void * VulkanIntegrationGetVulkanGetInstanceProcAddr() noexcept
{
    return nullptr;
}

int VulkanIntegrationEnumerateInstanceExtensions(
    char const *,
    VulkanIntegrationExtensionCallback,
    void *,
    int * out_count) noexcept
{
    if (out_count)
        *out_count = 0;
    return static_cast<int>(VulkanIntegrationResult::UNSUPPORTED_PLATFORM);
}

int VulkanIntegrationEnumerateInstanceLayers(
    VulkanIntegrationExtensionCallback,
    void *,
    int * out_count) noexcept
{
    if (out_count)
        *out_count = 0;
    return static_cast<int>(VulkanIntegrationResult::UNSUPPORTED_PLATFORM);
}

int VulkanIntegrationEnumerateDeviceExtensions(
    HonkordGL_GW_Handle,
    VulkanIntegrationExtensionCallback,
    void *,
    int * out_count) noexcept
{
    if (out_count)
        *out_count = 0;
    return static_cast<int>(VulkanIntegrationResult::UNSUPPORTED_PLATFORM);
}

} // namespace HonkordGL::Graphics
