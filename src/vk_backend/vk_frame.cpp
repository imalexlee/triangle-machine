#include "vk_frame.h"
#include "global_utils.h"
#include "vk_backend/vk_sync.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

void Frame::create(VkDevice device, uint32_t graphics_family_index) {
  render_semaphore = create_semaphore(device);
  present_semaphore = create_semaphore(device);
  render_fence = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

  command_context.create(device, graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  _deletion_queue.push_function([=, this]() {
    vkDestroySemaphore(device, render_semaphore, nullptr);
    vkDestroySemaphore(device, present_semaphore, nullptr);
    vkDestroyFence(device, render_fence, nullptr);
  });
}

void Frame::reset_sync_structures(VkDevice device) {
  _deletion_queue.flush();

  render_semaphore = create_semaphore(device);
  present_semaphore = create_semaphore(device);
  render_fence = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

  _deletion_queue.push_function([=, this]() {
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
