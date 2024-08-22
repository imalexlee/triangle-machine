#pragma once

#include "vk_backend/vk_device.h"
#include <vk_backend/vk_types.h>

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

class SwapchainContext {
  public:
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent;
    VkFormat format;
    VkSurfaceKHR surface;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;

    void create(VkInstance& instance, DeviceContext& device_controller, VkSurfaceKHR surface,
                VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_FIFO_KHR);
    void reset_swapchain(DeviceContext& device_context);
    void destroy();

  private:
    SwapchainSupportDetails _support_details;
    VkPresentModeKHR _present_mode;
    DeletionQueue _deletion_queue;

    SwapchainSupportDetails query_support_details(VkPhysicalDevice physical_device);
    void create_swapchain(DeviceContext& device_context);
    void destroy_swapchain(VkDevice device);
};
