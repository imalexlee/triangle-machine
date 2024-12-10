#pragma once

#include <vulkan/vulkan_core.h>

VkSemaphoreSubmitInfo vk_semaphore_submit_info_create(VkSemaphore semaphore, VkPipelineStageFlags2 stages, uint64_t timeline_value = 0);

VkSemaphore vk_semaphore_create(VkDevice device, VkSemaphoreType = VK_SEMAPHORE_TYPE_BINARY, uint64_t initial_timeline_value = 0);

VkFence vk_fence_create(VkDevice device, VkFenceCreateFlags flags);

void vk_image_memory_barrier_insert(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout,
                                    uint32_t mip_levels = 1, uint32_t base_mip_level = 0, uint32_t layer_count = 1,
                                    VkPipelineStageFlags2 src_stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                    VkPipelineStageFlags2 dst_stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
