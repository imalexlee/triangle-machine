#include "vk_frame.h"
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_sync.h"
#include <array>
#include <cstdint>
#include <vk_backend/vk_scene.h>
#include <vulkan/vulkan_core.h>

void Frame::create(VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index,
                   VkDescriptorSetLayout set_layout) {
  render_semaphore = create_semaphore(device);
  present_semaphore = create_semaphore(device);
  render_fence = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

  command_context.create(device, graphics_family_index,
                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  scene_data_buffer = create_buffer(
      sizeof(GlobalSceneData), allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

  std::array<PoolSizeRatio, 1> pool_sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}}};

  desc_allocator.create(device, 1, pool_sizes);

  global_desc_set = desc_allocator.allocate(device, set_layout);

  _deletion_queue.push_volatile([=, this]() {
    vkDestroySemaphore(device, render_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, render_fence, nullptr);
  });

  _deletion_queue.push_persistant([=, this]() { desc_allocator.destroy_pools(device); });
}

VkDescriptorSet Frame::create_scene_desc_set(VkDevice device, VkDescriptorSetLayout set_layout) {
  return desc_allocator.allocate(device, set_layout);
}

void Frame::clear_scene_desc_set(VkDevice device) { desc_allocator.clear_pools(device); }

void Frame::update_global_desc_set(VkDevice device, VmaAllocator allocator,
                                   GlobalSceneData scene_data) {
  vmaCopyMemoryToAllocation(allocator, &scene_data, scene_data_buffer.allocation, 0,
                            sizeof(GlobalSceneData));
  DescriptorWriter writer;
  writer.write_buffer(0, scene_data_buffer.buffer, sizeof(GlobalSceneData), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  writer.update_set(device, global_desc_set);
};

void Frame::reset_sync_structures(VkDevice device) {
  _deletion_queue.flush_volatile();

  render_semaphore = create_semaphore(device);
  present_semaphore = create_semaphore(device);
  render_fence = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

  _deletion_queue.push_volatile([=, this]() {
    vkDestroySemaphore(device, render_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, render_fence, nullptr);
  });
}

void Frame::destroy() {
  DEBUG_PRINT("destroying "
              "frame");
  command_context.destroy();
  _deletion_queue.flush();
}
