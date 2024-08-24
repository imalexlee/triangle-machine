#include "vk_buffer.h"
#include "vk_backend/vk_backend.h"

AllocatedBuffer create_buffer(VkDeviceSize             byte_size,
                              VmaAllocator             allocator,
                              VkBufferUsageFlags       buffer_usage,
                              VmaMemoryUsage           memory_usage,
                              VmaAllocationCreateFlags flags) {

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size  = byte_size;
    buffer_ci.usage = buffer_usage;

    VmaAllocationCreateInfo allocation_ci{};
    allocation_ci.usage = memory_usage;
    allocation_ci.flags = flags;

    AllocatedBuffer allocated_buffer{};
    VK_CHECK(vmaCreateBuffer(allocator, &buffer_ci, &allocation_ci, &allocated_buffer.buffer,
                             &allocated_buffer.allocation, &allocated_buffer.info));

    return allocated_buffer;
}

void destroy_buffer(VmaAllocator allocator, AllocatedBuffer* allocated_buffer) {
    vmaDestroyBuffer(allocator, allocated_buffer->buffer, allocated_buffer->allocation);
}

VkDeviceAddress get_buffer_device_address(VkDevice device, AllocatedBuffer buffer) {
    VkBufferDeviceAddressInfo device_address_info{};
    device_address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    device_address_info.buffer = buffer.buffer;

    return vkGetBufferDeviceAddress(device, &device_address_info);
}

MeshBuffers
upload_mesh_buffers(VkBackend* backend, std::span<uint32_t> indices, std::span<Vertex> vertices) {

    const size_t vertex_buffer_bytes = vertices.size() * sizeof(Vertex);
    const size_t index_buffer_bytes  = indices.size() * sizeof(uint32_t);

    AllocatedBuffer staging_buf =
        create_buffer(vertex_buffer_bytes + index_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, vertices.data(), staging_buf.allocation, 0,
                              vertex_buffer_bytes);

    vmaCopyMemoryToAllocation(backend->allocator, indices.data(), staging_buf.allocation,
                              vertex_buffer_bytes, index_buffer_bytes);

    MeshBuffers new_mesh_buffer;
    new_mesh_buffer.vertices =
        create_buffer(vertex_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0);
    new_mesh_buffer.indices =
        create_buffer(index_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0);

    immediate_submit(backend, [&](VkCommandBuffer cmd) {
        VkBufferCopy vertex_buffer_region{};
        vertex_buffer_region.size      = vertex_buffer_bytes;
        vertex_buffer_region.srcOffset = 0;
        vertex_buffer_region.dstOffset = 0;

        vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.vertices.buffer, 1,
                        &vertex_buffer_region);

        VkBufferCopy index_buffer_region{};
        index_buffer_region.size      = index_buffer_bytes;
        index_buffer_region.srcOffset = vertex_buffer_bytes;
        index_buffer_region.dstOffset = 0;

        vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.indices.buffer, 1,
                        &index_buffer_region);
    });

    destroy_buffer(backend->allocator, &staging_buf);
    return new_mesh_buffer;
}
