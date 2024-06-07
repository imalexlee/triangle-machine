#pragma once

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_command.h"
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

class Frame {
public:
  CommandContext command_context;
  VkSemaphore render_semaphore;
  VkSemaphore present_semaphore;
  VkFence render_fence;
  AllocatedBuffer scene_data_buffer;
  VkDescriptorSet global_desc_set;

  DescriptorAllocator desc_allocator;

  void create(VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index,
              VkDescriptorSetLayout global_desc_layout);

  VkDescriptorSet create_scene_desc_set(VkDevice device, VkDescriptorSetLayout set_layout);
  void clear_scene_desc_set(VkDevice device);
  void update_global_desc_set(VkDevice device, VmaAllocator allocator, GlobalSceneData scene_data);
  void destroy();
  void reset_sync_structures(VkDevice device);

private:
  DeletionQueue _deletion_queue;
};
