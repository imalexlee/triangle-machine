#pragma once
#include <span>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct AllocatedBuffer {
    VkBuffer          buffer;
    VmaAllocation     allocation;
    VmaAllocationInfo info;
};

struct MeshBuffers {
    AllocatedBuffer indices;
    AllocatedBuffer vertices;
};

/**
 * @brief Allocates a Vulkan buffer and its memory
 *
 * @param allocator             The VMA Allocator to allocate this memory with
 * @param byte_size             Size in bytes to allocate
 * @param buffer_usage          The usage of this buffer
 * @param memory_usage          The usage of this memory
 * @param flags                 Creation flags for this buffer
 * @param queue_family_indices  Queue family indices that this buffer will be accessed
 * @return                      An allocated buffer struct
 */
[[nodiscard]] AllocatedBuffer allocated_buffer_create(VmaAllocator allocator, VkDeviceSize byte_size, VkBufferUsageFlags buffer_usage,
                                                      VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags,
                                                      std::span<uint32_t> queue_family_indices = {});

/**
 * @brief Deallocates a Vulkan buffer and its memory
 *
 * @param allocator         The VMA Allocator which this buffer was allocated with
 * @param allocated_buffer  The AllocatedBuffer object to deallocate
 */
void allocated_buffer_destroy(VmaAllocator allocator, const AllocatedBuffer* allocated_buffer);

/**
 * @brief Retrieves a Vulkan Device Address given a Vulkan Buffer
 *
 * @param device The VkDevice associated with this buffer
 * @param buffer The vulkan buffer to retrieve an address for
 * @return       A GPU memory address of the buffer
 */
[[nodiscard]] VkDeviceAddress vk_buffer_device_address_get(VkDevice device, VkBuffer buffer);
