#include "vk_swapchain.h"
#include "global_utils.h"
#include "resources/vk_image.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_utils.h"
#include <cstdint>
#include <set>
#include <vulkan/vulkan_core.h>

// not to be used when resizing as it inits the entire context
void SwapchainContext::create(VkInstance& instance, DeviceContext& device_context, VkSurfaceKHR surface, uint32_t width,
                              uint32_t height, VkPresentModeKHR desired_present_mode) {
  this->surface = surface;
  _present_mode = desired_present_mode;

  // normal fifo is a good default unless ur an epic gamer
  if (desired_present_mode != VK_PRESENT_MODE_FIFO_KHR) {
    for (const auto& mode : _support_details.present_modes) {
      if (mode == desired_present_mode) {
        desired_present_mode = mode;
      }
    }
  }
  create_swapchain(device_context, width, height);

  _deletion_queue.push_function([=, this]() {
    destroy_swapchain(device_context.logical_device);
    vkDestroySurfaceKHR(instance, surface, nullptr);
  });
}

void SwapchainContext::reset_swapchain(DeviceContext& device_context, uint32_t width, uint32_t height) {
  destroy_swapchain(device_context.logical_device);
  create_swapchain(device_context, width, height);
}

void SwapchainContext::destroy() { _deletion_queue.flush(); }

void SwapchainContext::create_swapchain(DeviceContext& device_context, uint32_t width, uint32_t height) {
  _support_details = query_support_details(device_context.physical_device);
  extent = _support_details.capabilities.currentExtent;

  VkSurfaceFormatKHR surface_format = _support_details.formats[0];
  for (const auto& format : _support_details.formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = format;
    }
  }
  format = surface_format.format;

  // I want triple buffering
  uint32_t desired_image_count{3};
  // max image count of 0 means it's unbounded, so ignore this and go with the 3 images
  if (_support_details.capabilities.maxImageCount != 0) {
    // if 3 images are not available, just use the maximum amount of images possible
    desired_image_count = std::min(desired_image_count, _support_details.capabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR swapchain_ci{};
  swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_ci.minImageCount = desired_image_count;
  swapchain_ci.imageFormat = surface_format.format;
  swapchain_ci.imageColorSpace = surface_format.colorSpace;
  swapchain_ci.surface = surface;
  swapchain_ci.presentMode = _present_mode;
  swapchain_ci.imageExtent = extent;
  swapchain_ci.imageArrayLayers = 1;
  swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
  swapchain_ci.preTransform = _support_details.capabilities.currentTransform;
  swapchain_ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_ci.clipped = VK_TRUE;

  // try this out later
  // swapchain_ci.oldSwapchain = swapchain;

  const std::set<uint32_t> unique_queue_families{device_context.queues.graphics_family_index,
                                                 device_context.queues.present_family_index};
  std::vector<uint32_t> queue_family_indices;

  // if graphics and presentation operations are using seperate queue families, make swapchain share image data
  // across them. if they do share the same queue, using exlusive sharing mode is probably more performant
  if (unique_queue_families.size() > 1) {
    DEBUG_PRINT("using swapchain sharing mode CONCURRENT");
    queue_family_indices.resize(unique_queue_families.size());
    for (const auto& family_index : unique_queue_families) {
      queue_family_indices.push_back(family_index);
    }
    swapchain_ci.pQueueFamilyIndices = queue_family_indices.data();
    swapchain_ci.queueFamilyIndexCount = queue_family_indices.size();
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
  } else {
    DEBUG_PRINT("using swapchain sharing mode EXCLUSIVE");
    swapchain_ci.pQueueFamilyIndices = nullptr;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  VK_CHECK(vkCreateSwapchainKHR(device_context.logical_device, &swapchain_ci, nullptr, &swapchain));

  uint32_t actual_image_count;
  VK_CHECK(vkGetSwapchainImagesKHR(device_context.logical_device, swapchain, &actual_image_count, nullptr));

  DEBUG_PRINT("created %d images", actual_image_count);

  images.resize(actual_image_count);
  VK_CHECK(vkGetSwapchainImagesKHR(device_context.logical_device, swapchain, &actual_image_count, images.data()));

  for (auto& image : images) {
    image_views.push_back(create_image_view(device_context.logical_device, image, format, VK_IMAGE_ASPECT_COLOR_BIT));
  }
}

void SwapchainContext::destroy_swapchain(VkDevice device) {
  // this call implicitely destroys the VkImage's it gave us in create_swapchain
  vkDestroySwapchainKHR(device, swapchain, nullptr);
  for (const auto& image_view : image_views) {
    vkDestroyImageView(device, image_view, nullptr);
  }
  images.clear();
  image_views.clear();
}

SwapchainSupportDetails SwapchainContext::query_support_details(VkPhysicalDevice physical_device) {
  SwapchainSupportDetails swap_chain_details{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &swap_chain_details.capabilities));

  uint32_t surface_format_count{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, nullptr));

  if (surface_format_count > 0) {
    swap_chain_details.formats.resize(surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count,
                                                  swap_chain_details.formats.data()));
  }

  uint32_t present_modes_count{};
  VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, nullptr));

  if (present_modes_count > 0) {
    swap_chain_details.present_modes.resize(present_modes_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count,
                                                       swap_chain_details.present_modes.data()));
  }
  return swap_chain_details;
};
