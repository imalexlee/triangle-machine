#pragma once

#include <vk_backend/vk_debug.h>
#include <vulkan/vulkan_core.h>

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels = 1);

VkImageSubresourceRange create_image_subresource_range(VkImageAspectFlags aspect_flags, uint32_t mip_levels = 1);
