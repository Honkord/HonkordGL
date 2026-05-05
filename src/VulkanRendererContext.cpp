/**
 * @author Honkord <https://github.com>
 *
 * HonkordGL — Vulkan backend for RendererContext (Linux desktop)
 * Copyright (C) 2026 Honkord
 *
 * Link: -lvulkan (-lX11 -lwayland-client already pulled by window stack)
 */

#include <HonkordGL/RendererContext.h>
#include <HonkordGL/Video.h>
#include <HonkordGL/VulkanIntegration.h>
#include <HonkordGL/WindowApplication.h>

#if HONKORDGL_PLATFORM_LINUX_DESKTOP

# define VK_USE_PLATFORM_XLIB_KHR
# define VK_USE_PLATFORM_WAYLAND_KHR
# include <vulkan/vulkan.h>

# include <X11/Xlib.h>

# include <wayland-client.h>

# include "Wayland/WaylandWindowContext.hpp"

# include <algorithm>
# include <cstring>
# include <vector>

namespace HonkordGL::Graphics::Internal {

namespace {

struct VulkanRendererState {
    VkInstance instance{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue graphicsQueue{VK_NULL_HANDLE};
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat{};
    VkExtent2D swapchainExtent{};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkSemaphore imageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore renderFinishedSemaphore{VK_NULL_HANDLE};
    VkFence inFlightFence{VK_NULL_HANDLE};
    uint32_t graphicsFamily{VK_QUEUE_FAMILY_IGNORED};
    uint32_t presentFamily{VK_QUEUE_FAMILY_IGNORED};
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR_fn{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR_fn{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR_fn{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR_fn{nullptr};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR_fn{nullptr};
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR_fn{nullptr};
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR_fn{nullptr};
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR_fn{nullptr};
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR_fn{nullptr};
    PFN_vkQueuePresentKHR vkQueuePresentKHR_fn{nullptr};
};
bool InstanceExtensionAvailable(const char * name) noexcept
{
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
        return false;
    std::vector<VkExtensionProperties> props(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS)
        return false;
    for (const auto& p : props) {
        if (std::strcmp(p.extensionName, name) == 0)
            return true;
    }
    return false;
}
void LoadInstanceSurfaceProcs(VulkanRendererState& st) noexcept
{
    st.vkDestroySurfaceKHR_fn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(st.instance, "vkDestroySurfaceKHR"));
    st.vkGetPhysicalDeviceSurfaceSupportKHR_fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        vkGetInstanceProcAddr(st.instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    st.vkGetPhysicalDeviceSurfaceCapabilitiesKHR_fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        vkGetInstanceProcAddr(st.instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        vkGetInstanceProcAddr(st.instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    st.vkGetPhysicalDeviceSurfacePresentModesKHR_fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        vkGetInstanceProcAddr(st.instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
}
void LoadDeviceSwapchainProcs(VulkanRendererState& st) noexcept
{
    st.vkCreateSwapchainKHR_fn = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(st.device, "vkCreateSwapchainKHR"));
    st.vkDestroySwapchainKHR_fn = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(st.device, "vkDestroySwapchainKHR"));
    st.vkGetSwapchainImagesKHR_fn = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(st.device, "vkGetSwapchainImagesKHR"));
    st.vkAcquireNextImageKHR_fn = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(st.device, "vkAcquireNextImageKHR"));
    st.vkQueuePresentKHR_fn = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(st.device, "vkQueuePresentKHR"));
}

/** Destroys framebuffers, image views, and swapchain images list; optionally command pool. Swapchain handle is replaced via `vkCreateSwapchainKHR` (`oldSwapchain`) or destroyed in `DestroyAll`. */
void DestroySwapchainDerived(VulkanRendererState& st, bool destroyCommandPool) noexcept
{
    if (!st.device)
        return;
    vkDeviceWaitIdle(st.device);
    if (st.commandBuffer && st.commandPool) {
        vkFreeCommandBuffers(st.device, st.commandPool, 1, &st.commandBuffer);
        st.commandBuffer = VK_NULL_HANDLE;
    }
    if (destroyCommandPool && st.commandPool) {
        vkDestroyCommandPool(st.device, st.commandPool, nullptr);
        st.commandPool = VK_NULL_HANDLE;
    }
    for (VkFramebuffer fb : st.swapchainFramebuffers) {
        if (fb)
            vkDestroyFramebuffer(st.device, fb, nullptr);
    }
    st.swapchainFramebuffers.clear();
    for (VkImageView v : st.swapchainImageViews) {
        if (v)
            vkDestroyImageView(st.device, v, nullptr);
    }
    st.swapchainImageViews.clear();
    st.swapchainImages.clear();
}
void DestroyAll(VulkanRendererState& st) noexcept
{
    DestroySwapchainDerived(st, true);
    if (st.swapchain && st.vkDestroySwapchainKHR_fn)
        st.vkDestroySwapchainKHR_fn(st.device, st.swapchain, nullptr);
    st.swapchain = VK_NULL_HANDLE;
    if (st.renderPass && st.device)
        vkDestroyRenderPass(st.device, st.renderPass, nullptr);
    st.renderPass = VK_NULL_HANDLE;
    if (st.inFlightFence && st.device)
        vkDestroyFence(st.device, st.inFlightFence, nullptr);
    st.inFlightFence = VK_NULL_HANDLE;
    if (st.imageAvailableSemaphore && st.device)
        vkDestroySemaphore(st.device, st.imageAvailableSemaphore, nullptr);
    st.imageAvailableSemaphore = VK_NULL_HANDLE;
    if (st.renderFinishedSemaphore && st.device)
        vkDestroySemaphore(st.device, st.renderFinishedSemaphore, nullptr);
    st.renderFinishedSemaphore = VK_NULL_HANDLE;
    if (st.device)
        vkDestroyDevice(st.device, nullptr);
    st.device = VK_NULL_HANDLE;
    st.graphicsQueue = VK_NULL_HANDLE;
    if (st.surface && st.instance && st.vkDestroySurfaceKHR_fn)
        st.vkDestroySurfaceKHR_fn(st.instance, st.surface, nullptr);
    st.surface = VK_NULL_HANDLE;
    if (st.instance)
        vkDestroyInstance(st.instance, nullptr);
    st.instance = VK_NULL_HANDLE;
}
bool PickQueueFamilies(
    VulkanRendererState& st,
    VkPhysicalDevice phys,
    VkSurfaceKHR surf) noexcept
{
    st.graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
    st.presentFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
    std::vector<VkQueueFamilyProperties> fams(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, fams.data());
    for (uint32_t i = 0; i < count; ++i) {
        if ((fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            continue;
        VkBool32 present = VK_FALSE;
        if (st.vkGetPhysicalDeviceSurfaceSupportKHR_fn)
            st.vkGetPhysicalDeviceSurfaceSupportKHR_fn(phys, i, surf, &present);
        if (present) {
            st.graphicsFamily = i;
            st.presentFamily = i;
            return true;
        }
        if (st.graphicsFamily == VK_QUEUE_FAMILY_IGNORED)
            st.graphicsFamily = i;
    }
    if (st.graphicsFamily == VK_QUEUE_FAMILY_IGNORED)
        return false;
    for (uint32_t i = 0; i < count; ++i) {
        VkBool32 present = VK_FALSE;
        if (st.vkGetPhysicalDeviceSurfaceSupportKHR_fn)
            st.vkGetPhysicalDeviceSurfaceSupportKHR_fn(phys, i, surf, &present);
        if (present) {
            st.presentFamily = i;
            return true;
        }
    }
    return false;
}
bool CreateInstance(VulkanRendererState& st, std::vector<const char *>& outExt) noexcept
{
    outExt.clear();
    if (!InstanceExtensionAvailable(VK_KHR_SURFACE_EXTENSION_NAME))
        return false;
    outExt.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    if (InstanceExtensionAvailable(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
        outExt.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    if (InstanceExtensionAvailable(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
        outExt.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "HonkordGL";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "HonkordGL";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = static_cast<uint32_t>(outExt.size());
    ci.ppEnabledExtensionNames = outExt.data();

    return vkCreateInstance(&ci, nullptr, &st.instance) == VK_SUCCESS;
}
bool CreateSurface(ApplicationContextSettings& app, VulkanRendererState& st) noexcept
{
    const auto plat = static_cast<NativePlatform>(app.native_platform);
    if (plat == NativePlatform::Wayland) {
        auto * const wctx = reinterpret_cast<HonkordGL::Graphics::Wayland::Internal::WaylandWindowContext *>(app.window_handle);
        if (!wctx || !wctx->display || !wctx->surface)
            return false;
        VkWaylandSurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        sci.display = wctx->display;
        sci.surface = wctx->surface;
        auto const fn = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
            vkGetInstanceProcAddr(st.instance, "vkCreateWaylandSurfaceKHR"));
        return fn && fn(st.instance, &sci, nullptr, &st.surface) == VK_SUCCESS;
    }
    if (NativePlatformUsesX11ClientHandles(plat)) {
        auto * const dpy = reinterpret_cast<Display *>(app.device_context);
        const ::Window win = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(app.window_handle));
        if (!dpy || !win)
            return false;
        VkXlibSurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        sci.dpy = dpy;
        sci.window = win;
        auto const fn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
            vkGetInstanceProcAddr(st.instance, "vkCreateXlibSurfaceKHR"));
        return fn && fn(st.instance, &sci, nullptr, &st.surface) == VK_SUCCESS;
    }
    return false;
}
bool SelectPhysicalDevice(VulkanRendererState& st) noexcept
{
    uint32_t n = 0;
    if (vkEnumeratePhysicalDevices(st.instance, &n, nullptr) != VK_SUCCESS || n == 0)
        return false;
    std::vector<VkPhysicalDevice> devs(n);
    if (vkEnumeratePhysicalDevices(st.instance, &n, devs.data()) != VK_SUCCESS)
        return false;
    for (VkPhysicalDevice pd : devs) {
        if (!PickQueueFamilies(st, pd, st.surface))
            continue;
        uint32_t fmtCount = 0;
        if (st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn
            && st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn(pd, st.surface, &fmtCount, nullptr) == VK_SUCCESS
            && fmtCount > 0) {
            st.physicalDevice = pd;
            return true;
        }
    }
    return false;
}
bool CreateLogicalDevice(VulkanRendererState& st) noexcept
{
    const float qp = 1.f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = st.graphicsFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;

    std::vector<VkDeviceQueueCreateInfo> qcis;
    qcis.push_back(qci);
    if (st.presentFamily != st.graphicsFamily) {
        VkDeviceQueueCreateInfo q2 = qci;
        q2.queueFamilyIndex = st.presentFamily;
        qcis.push_back(q2);
    }

    const char * swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &swapExt;

    if (vkCreateDevice(st.physicalDevice, &dci, nullptr, &st.device) != VK_SUCCESS)
        return false;
    vkGetDeviceQueue(st.device, st.graphicsFamily, 0, &st.graphicsQueue);
    LoadDeviceSwapchainProcs(st);
    return st.vkCreateSwapchainKHR_fn && st.vkDestroySwapchainKHR_fn && st.vkGetSwapchainImagesKHR_fn
        && st.vkAcquireNextImageKHR_fn && st.vkQueuePresentKHR_fn;
}
bool CreateRenderPass(VulkanRendererState& st) noexcept
{
    VkAttachmentDescription color{};
    color.format = st.swapchainImageFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    return vkCreateRenderPass(st.device, &rpci, nullptr, &st.renderPass) == VK_SUCCESS;
}
bool PickSwapchainSurfaceFormat(VulkanRendererState& st) noexcept
{
    uint32_t fmtN = 0;
    if (!st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn
        || st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn(st.physicalDevice, st.surface, &fmtN, nullptr) != VK_SUCCESS
        || fmtN == 0)
        return false;
    std::vector<VkSurfaceFormatKHR> formats(fmtN);
    st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn(st.physicalDevice, st.surface, &fmtN, formats.data());
    st.swapchainImageFormat = formats[0].format;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_B8G8R8A8_UNORM) {
            st.swapchainImageFormat = f.format;
            break;
        }
    }
    return true;
}
bool CreateSwapchainFull(ApplicationContextSettings& app, VulkanRendererState& st) noexcept
{
    DestroySwapchainDerived(st, false);

    VkSwapchainKHR const oldChain = st.swapchain;

    VkSurfaceCapabilitiesKHR caps{};
    if (st.vkGetPhysicalDeviceSurfaceCapabilitiesKHR_fn(st.physicalDevice, st.surface, &caps) != VK_SUCCESS)
        return false;

    uint32_t fmtN = 0;
    st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn(st.physicalDevice, st.surface, &fmtN, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtN);
    st.vkGetPhysicalDeviceSurfaceFormatsKHR_fn(st.physicalDevice, st.surface, &fmtN, formats.data());

    if (caps.currentExtent.width != UINT32_MAX) {
        st.swapchainExtent = caps.currentExtent;
    } else {
        int w = app.frame_buffer_width > 0 ? app.frame_buffer_width : app.client_rect.w;
        int h = app.frame_buffer_height > 0 ? app.frame_buffer_height : app.client_rect.z;
        if (w <= 0)
            w = 640;
        if (h <= 0)
            h = 480;
        st.swapchainExtent.width = static_cast<uint32_t>(
            std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width));
        st.swapchainExtent.height = static_cast<uint32_t>(
            std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height));
    }

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount)
        imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = st.surface;
    sci.minImageCount = imgCount;
    sci.imageFormat = st.swapchainImageFormat;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto& f : formats) {
        if (f.format == st.swapchainImageFormat) {
            sci.imageColorSpace = f.colorSpace;
            break;
        }
    }
    sci.imageExtent = st.swapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t qf[] = {st.graphicsFamily, st.presentFamily};
    if (st.graphicsFamily != st.presentFamily) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = qf;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = oldChain;

    VkSwapchainKHR newChain = VK_NULL_HANDLE;
    if (st.vkCreateSwapchainKHR_fn(st.device, &sci, nullptr, &newChain) != VK_SUCCESS)
        return false;
    st.swapchain = newChain;

    uint32_t n = 0;
    st.vkGetSwapchainImagesKHR_fn(st.device, st.swapchain, &n, nullptr);
    st.swapchainImages.resize(n);
    st.vkGetSwapchainImagesKHR_fn(st.device, st.swapchain, &n, st.swapchainImages.data());

    st.swapchainImageViews.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VkImageViewCreateInfo ivci{};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = st.swapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = st.swapchainImageFormat;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(st.device, &ivci, nullptr, &st.swapchainImageViews[i]) != VK_SUCCESS)
            return false;
    }

    st.swapchainFramebuffers.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VkFramebufferCreateInfo fbci{};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = st.renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments = &st.swapchainImageViews[i];
        fbci.width = st.swapchainExtent.width;
        fbci.height = st.swapchainExtent.height;
        fbci.layers = 1;
        if (vkCreateFramebuffer(st.device, &fbci, nullptr, &st.swapchainFramebuffers[i]) != VK_SUCCESS)
            return false;
    }

    if (!st.commandPool) {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = st.graphicsFamily;
        if (vkCreateCommandPool(st.device, &cpci, nullptr, &st.commandPool) != VK_SUCCESS)
            return false;
    }

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = st.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    return vkAllocateCommandBuffers(st.device, &cbai, &st.commandBuffer) == VK_SUCCESS;
}

bool CreateSync(VulkanRendererState& st) noexcept
{
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(st.device, &sci, nullptr, &st.imageAvailableSemaphore) == VK_SUCCESS
        && vkCreateSemaphore(st.device, &sci, nullptr, &st.renderFinishedSemaphore) == VK_SUCCESS
        && vkCreateFence(st.device, &fci, nullptr, &st.inFlightFence) == VK_SUCCESS;
}

} // namespace

int AttachRendererContextVulkan(ApplicationContextSettings& app, const RendererContextSettings& spec) noexcept
{
    (void)spec;
    if (app.device || app.renderer_private)
        return static_cast<int>(RendererContextResult::ALREADY_ATTACHED);

    auto * const st = new (std::nothrow) VulkanRendererState();
    if (!st)
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);

    std::vector<const char *> ext;
    if (!CreateInstance(*st, ext)) {
        SetInternalApiError("Vulkan: vkCreateInstance failed or missing VK_KHR_surface.");
        delete st;
        return static_cast<int>(RendererContextResult::NO_DISPLAY);
    }

    LoadInstanceSurfaceProcs(*st);
    if (!CreateSurface(app, *st)) {
        SetInternalApiError("Vulkan: surface creation failed (need X11 or Wayland window).");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::NO_WINDOW);
    }

    if (!SelectPhysicalDevice(*st)) {
        SetInternalApiError("Vulkan: no suitable physical device.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!CreateLogicalDevice(*st)) {
        SetInternalApiError("Vulkan: vkCreateDevice failed.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!PickSwapchainSurfaceFormat(*st)) {
        SetInternalApiError("Vulkan: no surface formats.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    if (!CreateRenderPass(*st)) {
        SetInternalApiError("Vulkan: render pass creation failed.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    if (!CreateSwapchainFull(app, *st)) {
        SetInternalApiError("Vulkan: swapchain / framebuffers failed.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::PIXEL_FORMAT_FAILED);
    }

    if (!CreateSync(*st)) {
        SetInternalApiError("Vulkan: sync object creation failed.");
        DestroyAll(*st);
        delete st;
        return static_cast<int>(RendererContextResult::CONTEXT_CREATE_FAILED);
    }

    app.renderer_private = reinterpret_cast<HonkordGL_GW_Handle>(st);
    app.device = reinterpret_cast<HonkordGL_GW_Handle>(st->device);
    app.surface = reinterpret_cast<HonkordGL_GW_Handle>(st->surface);
    app.active_renderer = static_cast<int>(Renderers::VULKAN);
    app.color_bits = 32;
    app.depth_bits = 0;
    app.stencil_bits = 0;
    app.msaa_samples = 1;
    app.vsync_enabled = 1;
    app.frame_buffer_width = static_cast<int>(st->swapchainExtent.width);
    app.frame_buffer_height = static_cast<int>(st->swapchainExtent.height);

    return static_cast<int>(RendererContextResult::OK);
}
void DetachRendererContextVulkan(ApplicationContextSettings& app) noexcept
{
    auto * const st = reinterpret_cast<VulkanRendererState *>(app.renderer_private);
    if (!st) {
        app.device = nullptr;
        app.surface = nullptr;
        app.active_renderer = 0;
        return;
    }
    DestroyAll(*st);
    delete st;
    app.renderer_private = nullptr;
    app.device = nullptr;
    app.surface = nullptr;
    app.active_renderer = 0;
}
int RendererContextMakeCurrentVulkan(ApplicationContextSettings& app) noexcept
{
    (void)app;
    return static_cast<int>(RendererContextResult::OK);
}
void RendererContextSwapBuffersVulkan(ApplicationContextSettings& app) noexcept
{
    auto * const st = reinterpret_cast<VulkanRendererState *>(app.renderer_private);
    if (!st || !st->device || !st->swapchain)
        return;

    vkWaitForFences(st->device, 1, &st->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(st->device, 1, &st->inFlightFence);

    uint32_t imageIndex = 0;
    VkResult ar = st->vkAcquireNextImageKHR_fn(
        st->device, st->swapchain, UINT64_MAX, st->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (ar == VK_ERROR_OUT_OF_DATE_KHR || ar == VK_SUBOPTIMAL_KHR) {
        if (CreateSwapchainFull(app, *st)) {
            ar = st->vkAcquireNextImageKHR_fn(
                st->device, st->swapchain, UINT64_MAX, st->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        } else
            return;
    }
    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
        return;

    vkResetCommandBuffer(st->commandBuffer, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(st->commandBuffer, &bi);

    VkClearValue clear{};
    clear.color = {{{0.15f, 0.18f, 0.28f, 1.f}}};

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = st->renderPass;
    rpbi.framebuffer = st->swapchainFramebuffers[imageIndex];
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = st->swapchainExtent;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;

    vkCmdBeginRenderPass(st->commandBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(st->commandBuffer);
    vkEndCommandBuffer(st->commandBuffer);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &st->imageAvailableSemaphore;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &st->commandBuffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &st->renderFinishedSemaphore;

    if (vkQueueSubmit(st->graphicsQueue, 1, &si, st->inFlightFence) != VK_SUCCESS)
        return;

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &st->renderFinishedSemaphore;
    pi.swapchainCount = 1;
    pi.pSwapchains = &st->swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult pr = st->vkQueuePresentKHR_fn(st->graphicsQueue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
        (void)CreateSwapchainFull(app, *st);
}

int VulkanIntegrationFillHandles(ApplicationContextSettings const * app, VulkanIntegrationNativeHandles * out) noexcept
{
    if (!app || !out)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    auto * const st = reinterpret_cast<VulkanRendererState *>(app->renderer_private);
    if (!st || app->active_renderer != static_cast<int>(Renderers::VULKAN))
        return static_cast<int>(VulkanIntegrationResult::UNSUPPORTED_BACKEND);

    std::memset(out, 0, sizeof(*out));
    out->instance = reinterpret_cast<HonkordGL_GW_Handle>(st->instance);
    out->physical_device = reinterpret_cast<HonkordGL_GW_Handle>(st->physicalDevice);
    out->device = reinterpret_cast<HonkordGL_GW_Handle>(st->device);
    out->surface = reinterpret_cast<HonkordGL_GW_Handle>(st->surface);
    out->swapchain = reinterpret_cast<HonkordGL_GW_Handle>(st->swapchain);
    out->graphics_queue = reinterpret_cast<HonkordGL_GW_Handle>(st->graphicsQueue);
    out->render_pass = reinterpret_cast<HonkordGL_GW_Handle>(st->renderPass);
    out->command_pool = reinterpret_cast<HonkordGL_GW_Handle>(st->commandPool);
    out->command_buffer = reinterpret_cast<HonkordGL_GW_Handle>(st->commandBuffer);
    out->image_available_semaphore = reinterpret_cast<HonkordGL_GW_Handle>(st->imageAvailableSemaphore);
    out->render_finished_semaphore = reinterpret_cast<HonkordGL_GW_Handle>(st->renderFinishedSemaphore);
    out->flight_fence = reinterpret_cast<HonkordGL_GW_Handle>(st->inFlightFence);
    out->graphics_queue_family_index = st->graphicsFamily;
    out->present_queue_family_index = st->presentFamily;
    out->swapchain_width = st->swapchainExtent.width;
    out->swapchain_height = st->swapchainExtent.height;

    std::uint32_t const n = static_cast<std::uint32_t>(st->swapchainImages.size());
    out->swapchain_image_count = n;
    std::uint32_t const cap = static_cast<std::uint32_t>(sizeof(out->swapchain_images) / sizeof(out->swapchain_images[0]));
    std::uint32_t const m = n < cap ? n : cap;
    for (std::uint32_t i = 0; i < m; ++i) {
        out->swapchain_images[i] = reinterpret_cast<HonkordGL_GW_Handle>(st->swapchainImages[i]);
        out->swapchain_image_views[i] = reinterpret_cast<HonkordGL_GW_Handle>(st->swapchainImageViews[i]);
        out->swapchain_framebuffers[i] = reinterpret_cast<HonkordGL_GW_Handle>(st->swapchainFramebuffers[i]);
    }
    return static_cast<int>(VulkanIntegrationResult::OK);
}

} // namespace HonkordGL::Graphics::Internal

namespace HonkordGL::Graphics {

int VulkanIntegrationGetNativeHandles(ApplicationContextSettings const * app, VulkanIntegrationNativeHandles * out) noexcept
{
    return Internal::VulkanIntegrationFillHandles(app, out);
}

void * VulkanIntegrationGetInstanceProcAddr(HonkordGL_GW_Handle instance, char const * name) noexcept
{
    if (!name)
        return nullptr;
    return reinterpret_cast<void *>(vkGetInstanceProcAddr(reinterpret_cast<VkInstance>(instance), name));
}

void * VulkanIntegrationGetDeviceProcAddr(HonkordGL_GW_Handle device, char const * name) noexcept
{
    if (!name)
        return nullptr;
    return reinterpret_cast<void *>(vkGetDeviceProcAddr(reinterpret_cast<VkDevice>(device), name));
}

void * VulkanIntegrationGetVulkanGetInstanceProcAddr() noexcept
{
    return reinterpret_cast<void *>(&vkGetInstanceProcAddr);
}

int VulkanIntegrationEnumerateInstanceExtensions(
    char const * layer_name_or_null,
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept
{
    uint32_t count = 0;
    VkResult const q = vkEnumerateInstanceExtensionProperties(layer_name_or_null, &count, nullptr);
    if (q != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    if (!callback) {
        if (out_count)
            *out_count = static_cast<int>(count);
        return static_cast<int>(count);
    }
    std::vector<VkExtensionProperties> props(count);
    if (count > 0 && vkEnumerateInstanceExtensionProperties(layer_name_or_null, &count, props.data()) != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    int written = 0;
    for (uint32_t i = 0; i < count; ++i) {
        callback(user, props[i].extensionName, props[i].specVersion);
        ++written;
    }
    if (out_count)
        *out_count = written;
    return written;
}

int VulkanIntegrationEnumerateInstanceLayers(
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept
{
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    if (!callback) {
        if (out_count)
            *out_count = static_cast<int>(count);
        return static_cast<int>(count);
    }
    std::vector<VkLayerProperties> layers(count);
    if (count > 0 && vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    int written = 0;
    for (uint32_t i = 0; i < count; ++i) {
        callback(user, layers[i].layerName, layers[i].specVersion);
        ++written;
    }
    if (out_count)
        *out_count = written;
    return written;
}

int VulkanIntegrationEnumerateDeviceExtensions(
    HonkordGL_GW_Handle physical_device,
    VulkanIntegrationExtensionCallback callback,
    void * user,
    int * out_count) noexcept
{
    VkPhysicalDevice const pd = reinterpret_cast<VkPhysicalDevice>(physical_device);
    uint32_t count = 0;
    VkResult const q = vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr);
    if (q != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    if (!callback) {
        if (out_count)
            *out_count = static_cast<int>(count);
        return static_cast<int>(count);
    }
    std::vector<VkExtensionProperties> props(count);
    if (count > 0 && vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, props.data()) != VK_SUCCESS)
        return static_cast<int>(VulkanIntegrationResult::INVALID_ARGUMENT);
    int written = 0;
    for (uint32_t i = 0; i < count; ++i) {
        callback(user, props[i].extensionName, props[i].specVersion);
        ++written;
    }
    if (out_count)
        *out_count = written;
    return written;
}

} // namespace HonkordGL::Graphics

#else /* !linux desktop */

namespace HonkordGL::Graphics::Internal {

int AttachRendererContextVulkan(ApplicationContextSettings&, const RendererContextSettings&) noexcept
{
    return static_cast<int>(RendererContextResult::UNSUPPORTED_PLATFORM);
}
void DetachRendererContextVulkan(ApplicationContextSettings& app) noexcept
{
    app.renderer_private = nullptr;
    app.active_renderer = 0;
}
int RendererContextMakeCurrentVulkan(ApplicationContextSettings&) noexcept
{
    return static_cast<int>(RendererContextResult::NO_RENDERER_ATTACHED);
}
void RendererContextSwapBuffersVulkan(ApplicationContextSettings&) noexcept {}

} // namespace HonkordGL::Graphics::Internal

#endif