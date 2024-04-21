#include "vk_image.h"
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

// creates a 2D image along with its image_view
AllocatedImage create_image(VkDevice device, VmaAllocator allocator, VkImageUsageFlags usage, VkExtent2D extent,
                            VkFormat format) {

  VkExtent3D extent_3D{
      .width = extent.width,
      .height = extent.height,
      .depth = 1,
  };

  AllocatedImage allocated_image{};
  allocated_image.image_extent = extent_3D;
  allocated_image.image_format = format;

  VkImageCreateInfo image_ci{};
  image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_ci.extent = extent_3D;
  image_ci.format = format;
  image_ci.usage = usage;
  image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  image_ci.mipLevels = 1;
  image_ci.imageType = VK_IMAGE_TYPE_2D;
  image_ci.arrayLayers = 1;
  image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  //  image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocation_ci{};
  allocation_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocation_ci.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(allocator, &image_ci, &allocation_ci, &allocated_image.image, &allocated_image.allocation,
                          nullptr));

  VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  allocated_image.image_view = create_image_view(device, allocated_image.image, format, aspect_flag);

  return allocated_image;
}

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels) {

  VkImageView image_view;
  VkImageViewCreateInfo image_view_ci{};

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

void destroy_image(VkDevice device, VmaAllocator allocator, AllocatedImage& allocated_image) {
  vmaDestroyImage(allocator, allocated_image.image, allocated_image.allocation);
  vkDestroyImageView(device, allocated_image.image_view, nullptr);
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

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view, VkClearValue* clear) {
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView = view;
  colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  if (clear) {
    colorAttachment.clearValue = *clear;
  }

  return colorAttachment;
}

VkRenderingAttachmentInfo create_depth_attachment_info(VkImageView view) {
  VkRenderingAttachmentInfo depthAttachment{};
  depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depthAttachment.pNext = nullptr;
  depthAttachment.imageView = view;
  depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.clearValue.depthStencil.depth = 0.f;

  return depthAttachment;
}
