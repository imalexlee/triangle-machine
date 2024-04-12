#include "vk_frame.h"
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_sync.h"
#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

void Frame::create(VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index) {
  render_semaphore = create_semaphore(device);
  present_semaphore = create_semaphore(device);
  render_fence = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);
  command_context.create(device, graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  scene_data_buffer = create_buffer(
      sizeof(SceneData), allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
  // also check out using VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT since it caches buffer host-side

  DescriptorLayoutBuilder layout_builder;
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  desc_set_layout = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  std::array<PoolSizeRatio, 1> pool_sizes{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
  desc_allocator.create(device, 1, pool_sizes);

  _deletion_queue.push_volatile([=, this]() {
    vkDestroySemaphore(device, render_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, render_fence, nullptr);
  });

  _deletion_queue.push_persistant([=, this]() {
    vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
    desc_allocator.destroy_pools(device);
  });
}

VkDescriptorSet Frame::create_scene_desc_set(VkDevice device) {
  return desc_allocator.allocate(device, desc_set_layout);
}

void Frame::clear_scene_desc_set(VkDevice device) { desc_allocator.clear_pools(device); }

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
  DEBUG_PRINT("destroying frame");
  command_context.destroy();
  _deletion_queue.flush();
}
