#include "vk_image.h"
#include "vk_backend/vk_options.h"
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

// creates a 2D image along with its image_view
AllocatedImage create_image(VkDevice device, VmaAllocator allocator, VkImageUsageFlags usage, VkExtent2D extent,
                            VkFormat format, uint32_t samples) {

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
  image_ci.samples = (VkSampleCountFlagBits)samples;
  image_ci.mipLevels = 1;
  image_ci.imageType = VK_IMAGE_TYPE_2D;
  image_ci.arrayLayers = 1;
  image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

void blit_image(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent, VkExtent2D dst_extent) {
  VkImageBlit2 blit_region{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
  blit_region.srcOffsets[1].x = src_extent.width;
  blit_region.srcOffsets[1].y = src_extent.height;
  blit_region.srcOffsets[1].z = 1;

  blit_region.dstOffsets[1].x = dst_extent.width;
  blit_region.dstOffsets[1].y = dst_extent.height;
  blit_region.dstOffsets[1].z = 1;

  blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = 0;

  blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 blit_info{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};

  blit_info.dstImage = dest;
  blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blit_info.srcImage = src;
  blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blit_info.filter = VK_FILTER_LINEAR;
  blit_info.regionCount = 1;
  blit_info.pRegions = &blit_region;

  vkCmdBlitImage2(cmd, &blit_info);
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

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view, VkClearValue* clear,
                                                       VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op,
                                                       VkImageView resolve_img_view) {
  VkRenderingAttachmentInfo color_attachment{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.pNext = nullptr;
  color_attachment.imageView = view;
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = load_op;
  color_attachment.storeOp = store_op;
  if constexpr (vk_opts::msaa_enabled) {
    color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_attachment.resolveImageView = resolve_img_view;
    color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (clear) {
    color_attachment.clearValue = *clear;
  }

  return color_attachment;
}

VkRenderingAttachmentInfo create_depth_attachment_info(VkImageView view, VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op) {
  VkRenderingAttachmentInfo depth_attachment{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.pNext = nullptr;
  depth_attachment.imageView = view;
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = load_op;
  depth_attachment.storeOp = store_op;
  depth_attachment.clearValue.depthStencil.depth = 0.f;

  return depth_attachment;
}
