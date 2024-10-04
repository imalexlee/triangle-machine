#include "vk_swapchain.h"
#include "global_utils.h"
#include "resources/vk_image.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_utils.h"
#include <algorithm>
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>

[[nodiscard]] SwapchainSupportDetails query_support_details(const SwapchainContext* swapchain_ctx, VkPhysicalDevice physical_device);
void                                  create_swapchain(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx);
void                                  destroy_swapchain(SwapchainContext* swapchain_ctx, VkDevice device);

void swapchain_ctx_init(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx, VkSurfaceKHR surface,
                        VkPresentModeKHR desired_present_mode) {
    swapchain_ctx->surface      = surface;
    swapchain_ctx->present_mode = VK_PRESENT_MODE_FIFO_KHR; // fifo is guaranteed

    swapchain_ctx->support_details = query_support_details(swapchain_ctx, device_ctx->physical_device);
    for (const auto& mode : swapchain_ctx->support_details.present_modes) {
        if (mode == desired_present_mode) {
            swapchain_ctx->present_mode = mode;
        }
    }
    create_swapchain(swapchain_ctx, device_ctx);
}

void swapchain_ctx_reset(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx) {
    destroy_swapchain(swapchain_ctx, device_ctx->logical_device);
    create_swapchain(swapchain_ctx, device_ctx);
}

void swapchain_ctx_deinit(SwapchainContext* swapchain_ctx, VkDevice device, VkInstance instance) {
    destroy_swapchain(swapchain_ctx, device);
    vkDestroySurfaceKHR(instance, swapchain_ctx->surface, nullptr);
}

void create_swapchain(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx) {
    swapchain_ctx->support_details = query_support_details(swapchain_ctx, device_ctx->physical_device);
    swapchain_ctx->extent          = swapchain_ctx->support_details.capabilities.currentExtent;

    VkSurfaceFormatKHR surface_format = swapchain_ctx->support_details.formats[0];
    for (const auto& format : swapchain_ctx->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
        }
    }
    swapchain_ctx->format = surface_format.format;

    uint32_t desired_image_count = 3;

    // max image count of 0 means its unbounded.
    if (swapchain_ctx->support_details.capabilities.maxImageCount != 0) {
        desired_image_count = std::clamp(desired_image_count, swapchain_ctx->support_details.capabilities.minImageCount,
                                         swapchain_ctx->support_details.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchain_ci{};
    swapchain_ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.minImageCount    = desired_image_count;
    swapchain_ci.imageFormat      = surface_format.format;
    swapchain_ci.imageColorSpace  = surface_format.colorSpace;
    swapchain_ci.surface          = swapchain_ctx->surface;
    swapchain_ci.presentMode      = swapchain_ctx->present_mode;
    swapchain_ci.imageExtent      = swapchain_ctx->extent;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    swapchain_ci.preTransform     = swapchain_ctx->support_details.capabilities.currentTransform;
    swapchain_ci.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.clipped          = VK_TRUE;
    // try this out later
    // swapchain_ci.oldSwapchain
    // = swapchain;

    const std::set        unique_queue_families{device_ctx->queues.graphics_family_index, device_ctx->queues.present_family_index};
    std::vector<uint32_t> queue_family_indices;

    if (unique_queue_families.size() > 1) {
        DEBUG_PRINT("using swapchain sharing mode CONCURRENT");
        queue_family_indices.resize(unique_queue_families.size());
        for (const auto& family_index : unique_queue_families) {
            queue_family_indices.push_back(family_index);
        }
        swapchain_ci.pQueueFamilyIndices   = queue_family_indices.data();
        swapchain_ci.queueFamilyIndexCount = queue_family_indices.size();
        swapchain_ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    } else {
        DEBUG_PRINT("using swapchain sharing mode EXCLUSIVE");
        swapchain_ci.pQueueFamilyIndices   = nullptr;
        swapchain_ci.queueFamilyIndexCount = 0;
        swapchain_ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(device_ctx->logical_device, &swapchain_ci, nullptr, &swapchain_ctx->swapchain));

    uint32_t actual_image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(device_ctx->logical_device, swapchain_ctx->swapchain, &actual_image_count, nullptr));

    DEBUG_PRINT("created %d images", actual_image_count);

    swapchain_ctx->images.resize(actual_image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(device_ctx->logical_device, swapchain_ctx->swapchain, &actual_image_count, swapchain_ctx->images.data()));

    for (const auto& image : swapchain_ctx->images) {
        swapchain_ctx->image_views.push_back(
            vk_image_view_create(device_ctx->logical_device, image, VK_IMAGE_VIEW_TYPE_2D, swapchain_ctx->format, VK_IMAGE_ASPECT_COLOR_BIT));
    }
}

void destroy_swapchain(SwapchainContext* swapchain_ctx, VkDevice device) {
    // this call implicitly destroys the VkImage's it gave us in create_swapchain
    vkDestroySwapchainKHR(device, swapchain_ctx->swapchain, nullptr);
    for (const auto& image_view : swapchain_ctx->image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    swapchain_ctx->images.clear();
    swapchain_ctx->image_views.clear();
}

SwapchainSupportDetails query_support_details(const SwapchainContext* swapchain_ctx, VkPhysicalDevice physical_device) {
    SwapchainSupportDetails swap_chain_details{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, swapchain_ctx->surface, &swap_chain_details.capabilities));

    uint32_t surface_format_count{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, swapchain_ctx->surface, &surface_format_count, nullptr));

    if (surface_format_count > 0) {
        swap_chain_details.formats.resize(surface_format_count);
        VK_CHECK(
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, swapchain_ctx->surface, &surface_format_count, swap_chain_details.formats.data()));
    }

    uint32_t present_modes_count{};
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, swapchain_ctx->surface, &present_modes_count, nullptr));

    if (present_modes_count > 0) {
        swap_chain_details.present_modes.resize(present_modes_count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, swapchain_ctx->surface, &present_modes_count,
                                                           swap_chain_details.present_modes.data()));
    }
    return swap_chain_details;
}
