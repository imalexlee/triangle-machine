#include "vk_descriptor.h"
#include <vulkan/vulkan_core.h>

void DescriptorContext::create(DeviceContext& device_context, std::span<VkDescriptorPoolSize> pool_sizes) {
  VkDescriptorPoolCreateInfo pool_ci{};
  pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_ci.pPoolSizes = pool_sizes.data();
  pool_ci.poolSizeCount = pool_sizes.size();
}
