#include "vk_command.h"
#include "vk_backend/vk_utils.h"
#include <cstdint>
#include <fmt/format.h>
#include <thread>
#include <vulkan/vulkan_core.h>

void CommandContext::create(VkDevice device, uint32_t queue_index, VkCommandPoolCreateFlags flags) {

  uint32_t thread_count = std::thread::hardware_concurrency();
  _secondary_pools.resize(thread_count);

  VkCommandPoolCreateInfo command_pool_ci{};
  command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_ci.pNext = nullptr;
  command_pool_ci.queueFamilyIndex = queue_index;
  command_pool_ci.flags = flags;

  VK_CHECK(vkCreateCommandPool(device, &command_pool_ci, nullptr, &_primary_pool));

  for (uint32_t i = 0; i < thread_count; i++) {
    _secondary_pools[i].thread_idx = i;
    VK_CHECK(vkCreateCommandPool(device, &command_pool_ci, nullptr, &_secondary_pools[i].vk_pool));
  }

  VkCommandBufferAllocateInfo primary_buffer_ai{};
  primary_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  primary_buffer_ai.pNext = nullptr;
  primary_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  primary_buffer_ai.commandBufferCount = 1;
  primary_buffer_ai.commandPool = _primary_pool;

  VK_CHECK(vkAllocateCommandBuffers(device, &primary_buffer_ai, &primary_buffer));

  secondary_buffers.resize(thread_count);

  for (uint32_t i = 0; i < thread_count; i++) {

    VkCommandBufferAllocateInfo secondary_buffer_ai{};
    secondary_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    secondary_buffer_ai.pNext = nullptr;
    secondary_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    secondary_buffer_ai.commandBufferCount = 1;
    secondary_buffer_ai.commandPool = _secondary_pools[i].vk_pool;

    VK_CHECK(vkAllocateCommandBuffers(device, &secondary_buffer_ai, &secondary_buffers[i]));
  }

  _deletion_queue.push_persistant([=, this]() {
    for (const auto& pool : _secondary_pools) {
      vkDestroyCommandPool(device, pool.vk_pool, nullptr);
    }
    vkDestroyCommandPool(device, _primary_pool, nullptr);
  });
}

void CommandContext::destroy() { _deletion_queue.flush(); }

void CommandContext::begin_primary_buffer(VkCommandBufferUsageFlags flags) {

  VkCommandBufferBeginInfo command_buffer_bi{};
  command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_bi.pNext = nullptr;
  command_buffer_bi.flags = flags;

  VK_CHECK(vkBeginCommandBuffer(primary_buffer, &command_buffer_bi));
}

void CommandContext::submit_primary_buffer(VkQueue queue,
                                           VkSemaphoreSubmitInfo* wait_semaphore_info,
                                           VkSemaphoreSubmitInfo* signal_semaphore_info,
                                           VkFence fence) {

  VK_CHECK(vkEndCommandBuffer(primary_buffer));

  VkCommandBufferSubmitInfo command_buffer_si{};
  command_buffer_si.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_buffer_si.pNext = nullptr;
  command_buffer_si.deviceMask = 0;
  command_buffer_si.commandBuffer = primary_buffer;

  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.pNext = nullptr;
  submit_info.flags = 0;
  submit_info.pCommandBufferInfos = &command_buffer_si;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pWaitSemaphoreInfos = wait_semaphore_info;
  submit_info.waitSemaphoreInfoCount = wait_semaphore_info != nullptr ? 1 : 0;
  submit_info.pSignalSemaphoreInfos = signal_semaphore_info;
  submit_info.signalSemaphoreInfoCount = signal_semaphore_info != nullptr ? 1 : 0;

  VK_CHECK(vkQueueSubmit2(queue, 1, &submit_info, fence));
}
