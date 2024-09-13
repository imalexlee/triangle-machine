
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vk_backend/vk_backend.h"
#include <vk_backend/vk_sync.h>

// creates a 2D image along with its image_view
AllocatedImage create_image(VkDevice device, VmaAllocator allocator, VkImageUsageFlags usage,
                            VkExtent2D extent, VkFormat format, uint32_t samples) {

    VkExtent3D extent_3D{
        .width  = extent.width,
        .height = extent.height,
        .depth  = 1,
    };

    AllocatedImage allocated_image{};
    allocated_image.image_extent = extent_3D;
    allocated_image.image_format = format;

    VkImageCreateInfo image_ci{};
    image_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.extent        = extent_3D;
    image_ci.format        = format;
    image_ci.usage         = usage;
    image_ci.samples       = static_cast<VkSampleCountFlagBits>(samples);
    image_ci.mipLevels     = 1;
    image_ci.imageType     = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers   = 1;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_ci{};
    allocation_ci.usage         = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocation_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // creates an image handle and allocates the memory for it
    VK_CHECK(vmaCreateImage(allocator, &image_ci, &allocation_ci, &allocated_image.image,
                            &allocated_image.allocation, nullptr));

    // handle the case where we create a depth image
    VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    allocated_image.image_view =
        create_image_view(device, allocated_image.image, format, aspect_flag);

    return allocated_image;
}

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format,
                              VkImageAspectFlags aspect_flags, uint32_t mip_levels) {

    VkImageView           image_view;
    VkImageViewCreateInfo image_view_ci{};

    image_view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.pNext    = nullptr;
    image_view_ci.format   = format;
    image_view_ci.image    = image;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkImageSubresourceRange subresource_range =
        create_image_subresource_range(aspect_flags, mip_levels);

    image_view_ci.subresourceRange = subresource_range;

    VK_CHECK(vkCreateImageView(device, &image_view_ci, nullptr, &image_view));

    return image_view;
}

VkSampler create_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter,
                         VkSamplerAddressMode address_mode_u, VkSamplerAddressMode address_mode_v) {
    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.minFilter    = min_filter;
    sampler_ci.magFilter    = mag_filter;
    sampler_ci.addressModeU = address_mode_u;
    sampler_ci.addressModeV = address_mode_v;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(device, &sampler_ci, nullptr, &sampler));

    return sampler;
};

void blit_image(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent,
                VkExtent2D dst_extent) {
    VkImageBlit2 blit_region{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
    blit_region.srcOffsets[1].x = src_extent.width;
    blit_region.srcOffsets[1].y = src_extent.height;
    blit_region.srcOffsets[1].z = 1;

    blit_region.dstOffsets[1].x = dst_extent.width;
    blit_region.dstOffsets[1].y = dst_extent.height;
    blit_region.dstOffsets[1].z = 1;

    blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount     = 1;
    blit_region.srcSubresource.mipLevel       = 0;

    blit_region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount     = 1;
    blit_region.dstSubresource.mipLevel       = 0;

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

void destroy_image(VkDevice device, VmaAllocator allocator, const AllocatedImage* allocated_image) {
    vmaDestroyImage(allocator, allocated_image->image, allocated_image->allocation);
    vkDestroyImageView(device, allocated_image->image_view, nullptr);
}

VkImageSubresourceRange create_image_subresource_range(VkImageAspectFlags aspect_flags,
                                                       uint32_t           mip_levels) {
    VkImageSubresourceRange subresource_range{};
    subresource_range.aspectMask     = aspect_flags;
    subresource_range.baseMipLevel   = 0;
    subresource_range.levelCount     = mip_levels;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount     = 1;
    return subresource_range;
}

AllocatedImage upload_texture(const VkBackend* backend, const uint8_t* data,
                              VkImageUsageFlags usage, uint32_t color_channels, uint32_t width,
                              uint32_t height) {
    const VkExtent2D extent{.width = width, .height = height};

    const uint32_t byte_size = width * height * color_channels;

    AllocatedBuffer staging_buf = create_buffer(
        byte_size, backend->allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, data, staging_buf.allocation, 0, byte_size);

    const AllocatedImage new_texture =
        create_image(backend->device_ctx.logical_device, backend->allocator,
                     usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent, VK_FORMAT_R8G8B8A8_UNORM);

    VkBufferImageCopy copy_region;
    copy_region.bufferOffset      = 0;
    copy_region.bufferRowLength   = 0;
    copy_region.bufferImageHeight = 0;

    copy_region.imageOffset = {.x = 0, .y = 0, .z = 0};
    copy_region.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1};

    copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel       = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount     = 1;

    immediate_submit(backend, [&](VkCommandBuffer cmd) {
        insert_image_memory_barrier(cmd, new_texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        insert_image_memory_barrier(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroy_buffer(backend->allocator, &staging_buf);

    return new_texture;
}
