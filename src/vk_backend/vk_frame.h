#pragma once

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_command.h"
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

// contains per-frame shader information
struct SceneData {
  glm::mat4 view_proj{1.f};
};

class Frame {
public:
  CommandContext command_context;
  VkSemaphore render_semaphore;
  VkSemaphore present_semaphore;
  VkFence render_fence;
  AllocatedBuffer scene_data_buffer;
  // descriptor layout and allocator for the overall scene data
  VkDescriptorSetLayout desc_set_layout;
  DescriptorAllocator desc_allocator;

  void create(VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index);
  VkDescriptorSet create_scene_desc_set(VkDevice device);
  void clear_scene_desc_set(VkDevice device);
  void destroy();
  void reset_sync_structures(VkDevice device);

private:
  DeletionQueue _deletion_queue;
};
