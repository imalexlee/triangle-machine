#include "vk_image.h"
#include <vulkan/vulkan_core.h>

// operations for creating/handling data associated with vulkan images

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

  VkImageSubresourceRange subresource_range = create_image_subresource_range(aspect_flags, mip_levels);

  image_view_ci.subresourceRange = subresource_range;

  VK_CHECK(vkCreateImageView(device, &image_view_ci, nullptr, &image_view));

  return image_view;
}

VkImageSubresourceRange create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t mip_levels) {
  VkImageSubresourceRange subresource_range{};
  subresource_range.aspectMask = aspect_flags;
  subresource_range.baseMipLevel = 0;
  subresource_range.levelCount = mip_levels;
  subresource_range.baseArrayLayer = 0;
  subresource_range.layerCount = 1;

  return subresource_range;
}
