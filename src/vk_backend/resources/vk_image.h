#pragma once

#include <vk_backend/vk_types.h>

struct AllocatedImage {
  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;
  VkExtent3D image_extent;
  VkFormat image_format;
};

AllocatedImage create_image(VkDevice device, VmaAllocator allocator, VkImageUsageFlags usage, VkExtent2D extent,
                            VkFormat format, uint32_t samples = 1);

void blit_image(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent, VkExtent2D dst_extent);

void destroy_image(VkDevice device, VmaAllocator allocator, AllocatedImage& allocated_image);

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels = 1);

VkImageSubresourceRange create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t mip_levels = 1);

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view, VkClearValue* clear,
                                                       VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op);

VkRenderingAttachmentInfo create_depth_attachment_info(VkImageView view, VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op);
