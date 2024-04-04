#pragma once

#include "vk_backend/vk_command.h"
#include "vk_backend/vk_utils.h"
#include <cstdint>
#include <vk_backend/vk_types.h>

class Frame {
public:
  CommandContext command_context;
  VkSemaphore render_semaphore;
  VkSemaphore present_semaphore;
  VkFence render_fence;

  void create(VkDevice device, uint32_t graphics_family_index);
  void destroy();
  void reset_sync_structures(VkDevice device);

private:
  DeletionQueue _deletion_queue;
};
