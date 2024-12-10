#pragma once

#include "system/device/vk_types.h"
#include <vector>

struct VkContext;

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
 * @param swapchain_ctx		    The SwapchainContext to initialize
 * @param vk_ctx                Vulkan context for devices
 * @param desired_present_mode	The desired presentation mode
 */
void swapchain_ctx_init(SwapchainContext* swapchain_ctx, const VkContext* vk_ctx, VkPresentModeKHR desired_present_mode);

/**
 * @brief Destroys the current swapchain and creates a new
 *
 * @param swapchain_ctx	The swapchain context to reset
 * @param vk_ctx        Vulkan context for devices
 */
void swapchain_ctx_reset(SwapchainContext* swapchain_ctx, const VkContext* vk_ctx);

/**
 * @brief Destroys the swapchain and the surface associated with it
 *
 * @param swapchain_ctx	The swapchain context to deinitialize
 * @param vk_ctx        Vulkan context for devices
 */
void swapchain_ctx_deinit(SwapchainContext* swapchain_ctx, const VkContext* vk_ctx);
