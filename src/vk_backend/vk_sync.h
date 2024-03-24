#pragma once

#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

VkSemaphore create_semaphore(VkDevice device, VkSemaphoreType = VK_SEMAPHORE_TYPE_BINARY,
                             uint64_t initial_timeline_value = 0);

VkFence create_fence(VkDevice device, VkFenceCreateFlags flags);

VkSemaphoreSubmitInfo create_semaphore_submit_info(VkSemaphore semaphore, VkPipelineStageFlags2 stages,
                                                   uint64_t timeline_value = 0);

VkImageMemoryBarrier2
create_image_memory_barrier(VkImage image, VkImageLayout current_layout, VkImageLayout new_layout,
                            VkPipelineStageFlags2 src_stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                            VkPipelineStageFlags2 dst_stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

void insert_image_memory_barrier(VkCommandBuffer cmd_buf, VkImageMemoryBarrier2& image_barrier);
