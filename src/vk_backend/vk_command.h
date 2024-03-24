#pragma once

#include "vk_backend/vk_utils.h"
#include <vk_backend/vk_types.h>

// keeps track of a single primary buffer.
// in future will also coincide with related secondary buffers
class CommandContext {
public:
  VkCommandBuffer primary_buffer;

  void create(VkDevice device, uint32_t queue_index, VkCommandPoolCreateFlags flags);
  void begin_primary_buffer(VkCommandBufferUsageFlags flags);
  void submit_primary_buffer(VkQueue queue, VkSemaphoreSubmitInfo* wait_semaphore_info,
                             VkSemaphoreSubmitInfo* signal_semaphore_info, VkFence fence);
  void destroy();

private:
  VkCommandPool _pool;
  DeletionQueue _deletion_queue;

  void create_pool(VkDevice device, VkCommandPoolCreateFlags flags);
};
