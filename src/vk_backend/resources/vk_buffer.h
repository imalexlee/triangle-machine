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
 * @param byte_size             Size in bytes to allocate
 * @param allocator             The VMA Allocator to allocate this memory with
 * @param buffer_usage          The usage of this buffer
 * @param memory_usage          The usage of this memory
 * @param flags                 Creation flags for this buffer
 * @param queue_family_indices  Queue family indices that this buffer will be accessed
 * @return                      An allocated buffer struct
 */
[[nodiscard]] AllocatedBuffer create_buffer(VkDeviceSize byte_size, VmaAllocator allocator,
                                            VkBufferUsageFlags       buffer_usage,
                                            VmaMemoryUsage           memory_usage,
                                            VmaAllocationCreateFlags flags,
                                            std::span<uint32_t>      queue_family_indices = {});

/**
 * @brief Deallocates a Vulkan buffer and its memory
 *
 * @param allocator         The VMA Allocator which this buffer was allocated with
 * @param allocated_buffer  The AllocatedBuffer object to deallocate
 */
void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer* allocated_buffer);

/**
 * @brief Retrieves a Vulkan Device Address given an AllocatedBuffer object
 *
 * @param device The VkDevice associated with this buffer
 * @param buffer The AllocatedBuffer object to retrieve an address for
 * @return       A GPU memory address of the buffer
 */
[[nodiscard]] VkDeviceAddress get_buffer_device_address(VkDevice               device,
                                                        const AllocatedBuffer* buffer);
