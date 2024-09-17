#pragma once

#include <vk_backend/vk_types.h>

struct VkBackend;

struct AllocatedImage {
    VkImage       image;
    VkImageView   image_view;
    VmaAllocation allocation;
    VkExtent3D    image_extent;
    VkFormat      image_format;
};

struct TextureSampler {
    AllocatedImage tex;
    VkSampler      sampler;
};

[[nodiscard]] AllocatedImage create_image(VkDevice device, VmaAllocator allocator,
                                          VkImageUsageFlags usage, VkImageViewType view_type,
                                          VkExtent2D extent, VkFormat format, uint32_t samples = 1);

void blit_image(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent,
                VkExtent2D dst_extent, uint32_t src_layer_count, uint32_t dst_layer_count);

void destroy_image(VkDevice device, VmaAllocator allocator, const AllocatedImage* allocated_image);

[[nodiscard]] VkImageView create_image_view(VkDevice device, VkImage image,
                                            VkImageViewType view_type, VkFormat format,
                                            VkImageAspectFlags aspect_flags,
                                            uint32_t           mip_levels = 1);

[[nodiscard]] VkSampler
create_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter,
               VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
               VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT);

[[nodiscard]] VkImageSubresourceRange
create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t layer_count,
                               uint32_t mip_levels);

[[nodiscard]] AllocatedImage upload_texture(const VkBackend* backend, const uint8_t* data,
                                            VkImageUsageFlags usage, VkImageViewType view_type,
                                            uint32_t layer_count, uint32_t color_channels,
                                            uint32_t width, uint32_t height);
