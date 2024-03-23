#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
                              uint32_t mip_levels = 1);
