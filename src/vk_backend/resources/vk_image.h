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

/**
 * @brief Allocate an Vulkan Image, Image View, and Memory
 *
 * @param device    The device to allocate from
 * @param allocator The VMA Allocator to use for allocation
 * @param usage     The usage of the image
 * @param view_type The type of image view
 * @param extent    The extent of the image in 2D
 * @param format    The format of the image
 * @param samples   The number of samples for the image (for multisampling)
 * @return          An AllocatedImage
 */
[[nodiscard]] AllocatedImage create_image(VkDevice device, VmaAllocator allocator,
                                          VkImageUsageFlags usage, VkImageViewType view_type,
                                          VkExtent2D extent, VkFormat format, uint32_t samples = 1);

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
void blit_image(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D src_extent,
                VkExtent2D dst_extent, uint32_t src_layer_count, uint32_t dst_layer_count);

/**
 * @brief Deallocate a Vulkan Image, Image View, and Memory
 *
 * @param device          The device which the image was allocated
 * @param allocator       The VMA Allocator used to allocate this image
 * @param allocated_image The AllocatedImage to deallocate
 */
void destroy_image(VkDevice device, VmaAllocator allocator, const AllocatedImage* allocated_image);

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
[[nodiscard]] VkImageView create_image_view(VkDevice device, VkImage image,
                                            VkImageViewType view_type, VkFormat format,
                                            VkImageAspectFlags aspect_flags,
                                            uint32_t           mip_levels = 1);

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
[[nodiscard]] VkSampler
create_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter,
               VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
               VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT);

/**
 * @brief Creates a filled Vulkan Image Subresource Range
 *
 * @param aspect_flags Image aspect of the subresource range
 * @param layer_count  The layers in this subresource range
 * @param mip_levels   The mip levels in this subresource range
 * @return             a VkImageSubresourceRange
 */
[[nodiscard]] VkImageSubresourceRange
create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t layer_count,
                               uint32_t mip_levels);

/**
 * @brief Allocates a texture
 *
 * @param backend        The backend to allocate the texture for
 * @param data           The image data for the texture
 * @param usage          The usage type of the texture
 * @param view_type      The view type for the textures image view
 * @param layer_count    The number of layers this texture will have
 * @param color_channels The color channels for each texel
 * @param width          Width of the texture (or for one layer in a multi-layered texture)
 * @param height         Height of the texture (or for one layer in a multi-layered texture)
 * @return               An AllocatedImage representing a texture
 */
[[nodiscard]] AllocatedImage upload_texture(const VkBackend* backend, const uint8_t* data,
                                            VkImageUsageFlags usage, VkImageViewType view_type,
                                            uint32_t layer_count, uint32_t color_channels,
                                            uint32_t width, uint32_t height);
