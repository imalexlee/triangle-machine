#pragma once

#include <cstdint>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct CommandContext {
    VkCommandBuffer primary_buffer;
    VkCommandPool   primary_pool;
};

/**
 * @brief Initializes context for command pools and buffers
 *
 * @param cmd_ctx     CommandContext to init
 * @param device      Vulkan device to use to acquire command buffers from
 * @param queue_index Index of which queue we will submit these buffers to
 * @param flags	      Command pool creation usage
 */
void command_ctx_init(CommandContext* cmd_ctx, VkDevice device, uint32_t queue_index, VkCommandPoolCreateFlags flags);

/**
 * @brief Creates primary buffer info and begins command recording
 *
 * @param cmd_ctx CommandContext to begin recording from
 * @param flags	  Command pool usage
 */
void command_ctx_begin_primary_buffer(const CommandContext* cmd_ctx, VkCommandBufferUsageFlags flags);

/**
 * @brief Ends primary command buffer recording and submits buffer for execution
 *
 * @param cmd_ctx		CommandContext to submit from
 * @param queue			Vulkan queue to submit to
 * @param wait_semaphore_info	Semaphore to wait for to complete before executing
 * @param signal_semaphore_info Semaphore to signal once buffer is done
 * @param fence
 */
void command_ctx_submit_primary_buffer(const CommandContext* cmd_ctx, const VkQueue queue, const VkSemaphoreSubmitInfo* wait_semaphore_info,
                                       const VkSemaphoreSubmitInfo* signal_semaphore_info, const VkFence fence);

/**
 * @brief Deinitializes and frees command pools for this CommandContext
 *
 * @param cmd_ctx CommandContext to deinit
 * @param device  Vulkan Device used to free pools from
 */
void command_ctx_deinit(const CommandContext* cmd_ctx, VkDevice device);
