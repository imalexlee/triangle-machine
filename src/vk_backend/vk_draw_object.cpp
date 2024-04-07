#include "vk_draw_object.h"
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include <vulkan/vulkan_core.h>

void DrawObject::create(VkDevice& device, VmaAllocator& allocator, uint32_t indices_byte_len,
                        uint32_t vertices_byte_len) {

  vertex_buffer = create_buffer(vertices_byte_len, allocator,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                VMA_MEMORY_USAGE_GPU_ONLY, 0);
  DEBUG_PRINT("vertex buffer size: %d", (int)vertex_buffer.info.size);

  VkBufferDeviceAddressInfo device_address_info{};
  device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  device_address_info.buffer = vertex_buffer.buffer;

  push_constants.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);

  index_buffer =
      create_buffer(indices_byte_len, allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY, 0);
  DEBUG_PRINT("index buffer size: %d", (int)index_buffer.info.size);
}
