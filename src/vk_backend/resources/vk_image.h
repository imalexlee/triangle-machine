#pragma once

#include <vk_backend/vk_types.h>

struct AllocatedImage {
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkExtent3D image_extent;
  VkFormat image_format;
};

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels = 1);

VkImageSubresourceRange create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t mip_levels = 1);

VkRenderingAttachmentInfo create_rendering_attachment(VkImageView view, VkClearValue* clear,
                                                      VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

VkRenderingAttachmentInfo depth_attachment_info(VkImageView view,
                                                VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
