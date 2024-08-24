#pragma once
#include <span>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct VkBackend;
struct Vertex;

struct AllocatedBuffer {
    VkBuffer          buffer;
    VmaAllocation     allocation;
    VmaAllocationInfo info;
};

struct MeshBuffers {
    AllocatedBuffer indices;
    AllocatedBuffer vertices;
};

AllocatedBuffer create_buffer(VkDeviceSize             byte_size,
                              VmaAllocator             allocator,
                              VkBufferUsageFlags       buffer_usage,
                              VmaMemoryUsage           memory_usage,
                              VmaAllocationCreateFlags flags);

void destroy_buffer(VmaAllocator allocator, AllocatedBuffer* allocated_buffer);

VkDeviceAddress get_buffer_device_address(VkDevice device, AllocatedBuffer buffer);

MeshBuffers
upload_mesh_buffers(VkBackend* backend, std::span<uint32_t> indices, std::span<Vertex> vertices);
