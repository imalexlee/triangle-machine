#include "vk_buffer.h"
#include "vk_backend/vk_backend.h"

AllocatedBuffer create_buffer(VkDeviceSize byte_size, VmaAllocator allocator,
                              VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage,
                              VmaAllocationCreateFlags flags,
                              std::span<uint32_t>      queue_family_indices) {

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size  = byte_size;
    buffer_ci.usage = buffer_usage;
    buffer_ci.sharingMode =
        queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    buffer_ci.queueFamilyIndexCount = queue_family_indices.size();
    buffer_ci.pQueueFamilyIndices =
        queue_family_indices.size() > 1 ? queue_family_indices.data() : nullptr;

    VmaAllocationCreateInfo allocation_ci{};
    allocation_ci.usage = memory_usage;
    allocation_ci.flags = flags;

    AllocatedBuffer allocated_buffer{};
    VK_CHECK(vmaCreateBuffer(allocator, &buffer_ci, &allocation_ci, &allocated_buffer.buffer,
                             &allocated_buffer.allocation, &allocated_buffer.info));

    return allocated_buffer;
}

void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer* allocated_buffer) {
    vmaDestroyBuffer(allocator, allocated_buffer->buffer, allocated_buffer->allocation);
}

VkDeviceAddress get_buffer_device_address(VkDevice device, const AllocatedBuffer* buffer) {
    VkBufferDeviceAddressInfo device_address_info{};
    device_address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    device_address_info.buffer = buffer->buffer;

    return vkGetBufferDeviceAddress(device, &device_address_info);
}
