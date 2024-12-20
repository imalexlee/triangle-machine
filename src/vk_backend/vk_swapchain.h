#pragma once

#include "vk_backend/vk_device.h"
#include <vk_backend/vk_types.h>

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   present_modes;
};

struct SwapchainContext {
    VkSwapchainKHR           swapchain;
    VkExtent2D               extent;
    VkFormat                 format;
    VkSurfaceKHR             surface;
    SwapchainSupportDetails  support_details{};
    VkPresentModeKHR         present_mode;
    std::vector<VkImage>     images;
    std::vector<VkImageView> image_views;
};

/**
 * @brief Creates a swapchain along with associated state
 *
 * @param swapchain_ctx		The SwapchainContext to initialize
 * @param device_ctx		The device context to use for swapchain creation
 * @param surface		A surface to present to
 * @param desired_present_mode	The desired presentation mode
 */
void swapchain_ctx_init(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx, VkSurfaceKHR surface,
                        VkPresentModeKHR desired_present_mode);

/**
 * @brief Destroys the current swapchain and creates a new
 *
 * @param swapchain_ctx	The swapchain context to reset
 * @param device_ctx	The device to create a swapchain for
 */
void swapchain_ctx_reset(SwapchainContext* swapchain_ctx, const DeviceContext* device_ctx);

/**
 * @brief Destroys the swapchain and the surface associated with it
 *
 * @param swapchain_ctx	The swapchain context to deinitialize
 * @param device	The device associated with this swapchain
 * @param instance	The instance associated with this swapchain
 */
void swapchain_ctx_deinit(SwapchainContext* swapchain_ctx, VkDevice device, VkInstance instance);
