#pragma once
#include <vk_backend/vk_types.h>

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

AllocatedBuffer create_buffer(VkDeviceSize size, VmaAllocator allocator, VkBufferUsageFlags buffer_usage,
                              VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags);

void destroy_buffer(VmaAllocator& allocator, AllocatedBuffer& allocated_buffer);
