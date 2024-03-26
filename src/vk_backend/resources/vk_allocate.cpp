#define VMA_IMPLEMENTATION

#include "vk_allocate.h"

VmaAllocator create_allocator(VkInstance instance, DeviceContext& device_context) {
  VmaAllocator allocator;

  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.device = device_context.logical_device;
  allocator_info.physicalDevice = device_context.physical_device;
  allocator_info.instance = instance;
  allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator));

  return allocator;
}
