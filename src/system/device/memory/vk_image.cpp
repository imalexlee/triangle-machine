
#define STB_IMAGE_IMPLEMENTATION
#include "vk_image.h"
#include "stb_image.h"
#include <system/device/vk_types.h>

// creates a 2D image along with its image_view
AllocatedImage allocated_image_create(VkDevice device, VmaAllocator allocator, VkImageUsageFlags img_usage, VkImageViewType view_type,
                                      VkExtent2D extent, VkFormat format, uint32_t mip_levels, uint32_t samples, VmaMemoryUsage memory_usage,
                                      VmaAllocationCreateFlags allocation_flags) {

    VkExtent3D extent_3D{
        .width  = extent.width,
        .height = extent.height,
        .depth  = 1,
    };

    AllocatedImage allocated_image{};
    allocated_image.image_extent = extent_3D;
    allocated_image.image_format = format;

    // uint32_t mip_levels        = std::floor(std::log2(std::max(extent.width, extent.height))) + 1;
    allocated_image.mip_levels = mip_levels;

    VkImageCreateInfo image_ci{};
    image_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.extent        = extent_3D;
    image_ci.format        = format;
    image_ci.usage         = img_usage;
    image_ci.samples       = static_cast<VkSampleCountFlagBits>(samples);
    image_ci.mipLevels     = allocated_image.mip_levels;
    image_ci.imageType     = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers   = view_type == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.flags         = view_type == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VmaAllocationCreateInfo allocation_ci{};
    allocation_ci.usage = memory_usage;
    allocation_ci.flags = allocation_flags;

    // creates an image handle and allocates the memory for it
    VK_CHECK(vmaCreateImage(allocator, &image_ci, &allocation_ci, &allocated_image.image, &allocated_image.allocation, nullptr));

    // handle the case where we create a depth image
    VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    allocated_image.image_view = vk_image_view_create(device, allocated_image.image, view_type, format, aspect_flag, allocated_image.mip_levels);

    return allocated_image;
}

VkImageView vk_image_view_create(VkDevice device, VkImage image, VkImageViewType view_type, VkFormat format, VkImageAspectFlags aspect_flags,
                                 uint32_t mip_levels) {

    VkImageView           image_view;
    VkImageViewCreateInfo image_view_ci{};

    image_view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.pNext    = nullptr;
    image_view_ci.format   = format;
    image_view_ci.image    = image;
    image_view_ci.viewType = view_type;

    uint32_t layer_count = view_type == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;

    VkImageSubresourceRange subresource_range = vk_image_subresource_range_create(aspect_flags, layer_count, mip_levels, 0);

    image_view_ci.subresourceRange = subresource_range;

    VK_CHECK(vkCreateImageView(device, &image_view_ci, nullptr, &image_view));

    return image_view;
}

VkImage vk_image_create(VkDevice device, VkFormat format, VkImageUsageFlags usage, uint32_t width, uint32_t height, uint32_t layer_count,
                        uint32_t samples, uint32_t mip_levels) {

    VkImageCreateInfo image_ci{};
    image_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType     = VK_IMAGE_TYPE_2D;
    image_ci.extent.width  = width;
    image_ci.extent.height = height;
    image_ci.extent.depth  = 1;
    image_ci.mipLevels     = mip_levels;
    image_ci.arrayLayers   = layer_count;
    image_ci.format        = format;
    image_ci.tiling        = VK_IMAGE_TILING_LINEAR;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.usage         = usage;
    image_ci.samples       = static_cast<VkSampleCountFlagBits>(samples);
    image_ci.flags         = layer_count == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VkImage image;
    VK_CHECK(vkCreateImage(device, &image_ci, nullptr, &image));
    return image;
}

VkSampler vk_sampler_create(VkDevice device, VkFilter min_filter, VkFilter mag_filter, float min_lod, float max_lod,
                            VkSamplerAddressMode address_mode_u, VkSamplerAddressMode address_mode_v) {
    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.minFilter    = min_filter;
    sampler_ci.magFilter    = mag_filter;
    sampler_ci.addressModeU = address_mode_u;
    sampler_ci.addressModeV = address_mode_v;
    sampler_ci.minLod       = min_lod;
    sampler_ci.maxLod       = max_lod;
    sampler_ci.mipLodBias   = max_lod > 0.f ? -1.f : 0.f;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(device, &sampler_ci, nullptr, &sampler));

    return sampler;
};

void vk_image_blit(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent, VkExtent2D dst_extent, uint32_t src_mip_level,
                   uint32_t dst_mip_level, uint32_t src_layer_count, uint32_t dst_layer_count) {
    VkImageBlit2 blit_region{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
    blit_region.srcOffsets[0]   = {0, 0, 0};
    blit_region.srcOffsets[1].x = src_extent.width;
    blit_region.srcOffsets[1].y = src_extent.height;
    blit_region.srcOffsets[1].z = 1;

    blit_region.dstOffsets[0]   = {0, 0, 0};
    blit_region.dstOffsets[1].x = dst_extent.width;
    blit_region.dstOffsets[1].y = dst_extent.height;
    blit_region.dstOffsets[1].z = 1;

    blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount     = src_layer_count;
    blit_region.srcSubresource.mipLevel       = src_mip_level;

    blit_region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount     = dst_layer_count;
    blit_region.dstSubresource.mipLevel       = dst_mip_level;

    VkBlitImageInfo2 blit_info{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};

    blit_info.dstImage       = dest;
    blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blit_info.srcImage       = src;
    blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_info.filter         = VK_FILTER_LINEAR;
    blit_info.regionCount    = 1;
    blit_info.pRegions       = &blit_region;

    vkCmdBlitImage2(cmd, &blit_info);
}

void allocated_image_destroy(VkDevice device, VmaAllocator allocator, const AllocatedImage* allocated_image) {
    vmaDestroyImage(allocator, allocated_image->image, allocated_image->allocation);
    vkDestroyImageView(device, allocated_image->image_view, nullptr);
}

VkImageSubresourceRange vk_image_subresource_range_create(VkImageAspectFlags aspect_flags, uint32_t layer_count, uint32_t mip_levels,
                                                          uint32_t base_mip_level) {
    VkImageSubresourceRange subresource_range{};
    subresource_range.aspectMask     = aspect_flags;
    subresource_range.baseMipLevel   = base_mip_level;
    subresource_range.levelCount     = mip_levels;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount     = layer_count;
    return subresource_range;
}
