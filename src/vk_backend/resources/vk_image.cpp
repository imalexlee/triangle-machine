#define VMA_IMPLEMENTATION

#include "vk_image.h"

// operations for creating/handling data associated with vulkan images

AllocatedImage create_image(VkDevice device, VmaAllocator allocator, VkImageUsageFlags usage, VkExtent3D extent,
                            VkFormat format) {

  VkImage image;
  VkImageCreateInfo image_ci{};
  image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_ci.extent = extent;
  image_ci.format = format;
  image_ci.usage = usage;
  image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  image_ci.mipLevels = 1;

  VmaAllocationCreateInfo allocation_ci{};
  allocation_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocation_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  AllocatedImage allocated_image{};
  allocated_image.image_extent = extent;
  allocated_image.image_format = format;
  allocated_image.image = image;
  allocated_image.image_view = create_image_view(device, image, format, aspect_flag);

  vmaCreateImage(allocator, &image_ci, &allocation_ci, &image, &allocated_image.allocation, nullptr);

  return allocated_image;
}

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

VkRenderingAttachmentInfo create_rendering_attachment(VkImageView view, VkClearValue* clear, VkImageLayout layout) {
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView = view;
  colorAttachment.imageLayout = layout;
  colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear) {
    colorAttachment.clearValue = *clear;
  }

  return colorAttachment;
}

VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout) {
  VkRenderingAttachmentInfo depthAttachment{};
  depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depthAttachment.pNext = nullptr;
  depthAttachment.imageView = view;
  depthAttachment.imageLayout = layout;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.clearValue.depthStencil.depth = 0.f;

  return depthAttachment;
}
