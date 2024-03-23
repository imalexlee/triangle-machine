#include "vk_image.h"
#include <vulkan/vulkan_core.h>

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels) {

  VkImageView image_view;
  VkImageViewCreateInfo image_view_ci{};
  uint32_t mip_level_count{1};

  image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_ci.pNext = nullptr;
  image_view_ci.format = format;
  image_view_ci.image = image;
  image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_ci.subresourceRange.aspectMask = aspect_flags;
  image_view_ci.subresourceRange.baseMipLevel = 0;
  image_view_ci.subresourceRange.baseArrayLayer = 0;
  image_view_ci.subresourceRange.layerCount = 1;
  image_view_ci.subresourceRange.levelCount = mip_levels;

  VK_CHECK(vkCreateImageView(device, &image_view_ci, nullptr, &image_view));

  return image_view;
}
