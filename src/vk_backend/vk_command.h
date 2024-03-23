#pragma once
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

// keeps track of a single primary buffer.
// in future will also coincide with related secondary buffers
class CommandBufferController {
public:
  VkCommandBuffer primary_buffer;

  void create_command_buffers(VkDevice device, uint32_t queue_index, VkCommandPoolCreateFlags flags);
  void begin_primary_buffer(VkCommandBufferUsageFlags flags);
  void submit_primary_buffer(VkQueue queue, VkSemaphoreSubmitInfo* wait_semaphore_info,
                             VkSemaphoreSubmitInfo* signal_semaphore_info, VkFence fence);
  void destroy_command_buffers(VkDevice device);

private:
  VkCommandPool _pool;

  void create_pool(VkDevice device, VkCommandPoolCreateFlags flags);
};
