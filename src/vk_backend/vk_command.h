#pragma once

#include "vk_backend/vk_utils.h"
#include <cstdint>
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct CommandPool {
  VkCommandPool vk_pool;
  uint8_t thread_idx;
};

class CommandContext {
public:
  VkCommandBuffer primary_buffer;
  std::vector<VkCommandBuffer> secondary_buffers;

  void create(VkDevice device, uint32_t queue_index, VkCommandPoolCreateFlags flags);
  void begin_primary_buffer(VkCommandBufferUsageFlags flags);
  void submit_primary_buffer(VkQueue queue, VkSemaphoreSubmitInfo* wait_semaphore_info,
                             VkSemaphoreSubmitInfo* signal_semaphore_info, VkFence fence);
  void destroy();

private:
  VkCommandPool _primary_pool;
  std::vector<CommandPool> _secondary_pools;
  DeletionQueue _deletion_queue;

  void create_pool(VkDevice device, VkCommandPoolCreateFlags flags);
};
