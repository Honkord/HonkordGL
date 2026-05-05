# Vulkan backend

## Scope

Swapchain/renderer context on Linux desktop and Apple when enabled; Windows uses noop/stub integration unless extended.

## Requirements

Vulkan loader/SDK (`libvulkan-dev` on Debian-like systems).

## CMake

`HONKORDGL_ENABLE_VULKAN`: `AUTO`, `ON`, or `OFF`. When unavailable, `VulkanNoop.cpp` and `VulkanIntegrationStubs.cpp` provide predictable failures.

## Public surface

`VulkanIntegration.h` — functions return documented error codes when unsupported.

## Capability flag

`HONKORDGL_HAVE_VULKAN` / `IsVulkanBackendAvailable()`.
