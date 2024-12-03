#pragma once

#include <vk_backend/vk_types.h>

struct AllocatedImage {
    VkImage       image;
    VkImageView   image_view;
    VmaAllocation allocation;
    VkExtent3D    image_extent;
    VkFormat      image_format;
    uint32_t      mip_levels{1};
};

struct MipLevel {
    void*    data;
    uint32_t width;
    uint32_t height;
};

struct TextureSampler {
    // const uint8_t*  data{};
    VkSampler       sampler{};
    VkImageViewType view_type{};
    uint32_t        layer_count{1};
    // uint32_t        mip_count{1};
    uint32_t color_channels{};
    uint32_t width{};
    uint32_t height{};
    // if mip mapped, will be size of all mips
    // uint32_t              byte_size{};
    std::vector<MipLevel> mip_levels;
};

/**
 * @brief Allocate a Vulkan Image, Image View, and Memory
 *
 * @param device    The device to allocate from
 * @param allocator The VMA Allocator to use for allocation
 * @param img_usage The usage of the image
 * @param view_type The type of image view
 * @param extent    The extent of the image in 2D
 * @param format    The format of the image
 * @param samples   The number of samples for the image (for multisampling)
 * @return          An AllocatedImage
 */
AllocatedImage allocated_image_create(VkDevice device, VmaAllocator allocator, VkImageUsageFlags img_usage, VkImageViewType view_type,
                                      VkExtent2D extent, VkFormat format, uint32_t mip_levels, uint32_t samples = 1,
                                      VmaMemoryUsage           memory_usage     = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                      VmaAllocationCreateFlags allocation_flags = 0);

/**
 * @brief Perform a blit (copy) from one image to another
 *
 * @param cmd             The command buf to record the blit command onto
 * @param src             Source image
 * @param dest            Destination image
 * @param src_extent      Extent of source in 2D
 * @param dst_extent      Extent of destination in 2D
 * @param src_layer_count Layers of source image
 * @param dst_layer_count Layers of destination image
 */
void vk_image_blit(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent, VkExtent2D dst_extent, uint32_t src_mip_level = 0,
                   uint32_t dst_mip_level = 0, uint32_t src_layer_count = 1, uint32_t dst_layer_count = 1);

/**
 * @brief Deallocate a Vulkan Image, Image View, and Memory
 *
 * @param device          The device which the image was allocated
 * @param allocator       The VMA Allocator used to allocate this image
 * @param allocated_image The AllocatedImage to deallocate
 */
void allocated_image_destroy(VkDevice device, VmaAllocator allocator, const AllocatedImage* allocated_image);

/**
 * @brief Creates a raw Vulkan Image View
 *
 * @param device       The device to create an image view from
 * @param image        The underlying image for this image view
 * @param view_type    Type of image view
 * @param format       Format of image view
 * @param aspect_flags Aspect flags for the image view
 * @param mip_levels   Amount of mip levels
 * @return             A VkImageView
 */
[[nodiscard]] VkImageView vk_image_view_create(VkDevice device, VkImage image, VkImageViewType view_type, VkFormat format,
                                               VkImageAspectFlags aspect_flags, uint32_t mip_levels = 1);

[[nodiscard]] VkImage vk_image_create(VkDevice device, VkFormat format, VkImageUsageFlags usage, uint32_t width, uint32_t height,
                                      uint32_t layer_count = 1, uint32_t samples = 1, uint32_t mip_levels = 1);

/**
 * @brief Creates a raw Vulkan Sampler
 *
 * @param device         The device to create the sampler from
 * @param min_filter     Minification filter
 * @param mag_filter     Magnification filter
 * @param address_mode_u Addressing mode for U coords outside [0,1)
 * @param address_mode_v Addressing mode for V coords outside [0,1)
 * @return               A VkSampler
 */
[[nodiscard]] VkSampler vk_sampler_create(VkDevice device, VkFilter min_filter, VkFilter mag_filter, float min_lod = 0.f, float max_lod = 0.f,
                                          VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT);

/**
 * @brief Creates a filled Vulkan Image Subresource Range
 *
 * @param aspect_flags   Image aspect of the subresource range
 * @param layer_count    The layers in this subresource range
 * @param mip_levels     The mip levels in this subresource range
 * @param base_mip_level Mip level for subresource
 * @return               a VkImageSubresourceRange
 */
[[nodiscard]] VkImageSubresourceRange vk_image_subresource_range_create(VkImageAspectFlags aspect_flags, uint32_t layer_count, uint32_t mip_levels,
                                                                        uint32_t base_mip_level);
