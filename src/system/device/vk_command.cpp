
#include "vk_types.h"
#include <cstdint>
#include <thread>

#include "vk_command.h"

void command_ctx_init(CommandContext* cmd_ctx, VkDevice device, uint32_t queue_family_index, VkCommandPoolCreateFlags flags) {

    VkCommandPoolCreateInfo command_pool_ci{};
    command_pool_ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.pNext            = nullptr;
    command_pool_ci.queueFamilyIndex = queue_family_index;
    command_pool_ci.flags            = flags;

    VK_CHECK(vkCreateCommandPool(device, &command_pool_ci, nullptr, &cmd_ctx->primary_pool));

    VkCommandBufferAllocateInfo primary_buffer_ai{};
    primary_buffer_ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    primary_buffer_ai.pNext              = nullptr;
    primary_buffer_ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    primary_buffer_ai.commandBufferCount = 1;
    primary_buffer_ai.commandPool        = cmd_ctx->primary_pool;

    VK_CHECK(vkAllocateCommandBuffers(device, &primary_buffer_ai, &cmd_ctx->primary_buffer));
}

void command_ctx_deinit(const CommandContext* cmd_ctx, VkDevice device) { vkDestroyCommandPool(device, cmd_ctx->primary_pool, nullptr); }

void command_ctx_begin_primary_buffer(const CommandContext* cmd_ctx, VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo command_buffer_bi{};
    command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_bi.pNext = nullptr;
    command_buffer_bi.flags = flags;

    VK_CHECK(vkBeginCommandBuffer(cmd_ctx->primary_buffer, &command_buffer_bi));
}

void command_ctx_submit_primary_buffer(const CommandContext* cmd_ctx, VkQueue queue, VkFence fence, const VkSemaphoreSubmitInfo* wait_semaphore_info,
                                       const VkSemaphoreSubmitInfo* signal_semaphore_info) {
    VK_CHECK(vkEndCommandBuffer(cmd_ctx->primary_buffer));

    VkCommandBufferSubmitInfo command_buffer_si{};
    command_buffer_si.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_si.pNext         = nullptr;
    command_buffer_si.deviceMask    = 0;
    command_buffer_si.commandBuffer = cmd_ctx->primary_buffer;

    VkSubmitInfo2 submit_info{};
    submit_info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.pNext                    = nullptr;
    submit_info.flags                    = 0;
    submit_info.pCommandBufferInfos      = &command_buffer_si;
    submit_info.commandBufferInfoCount   = 1;
    submit_info.pWaitSemaphoreInfos      = wait_semaphore_info;
    submit_info.waitSemaphoreInfoCount   = wait_semaphore_info != nullptr ? 1 : 0;
    submit_info.pSignalSemaphoreInfos    = signal_semaphore_info;
    submit_info.signalSemaphoreInfoCount = signal_semaphore_info != nullptr ? 1 : 0;

    VK_CHECK(vkQueueSubmit2(queue, 1, &submit_info, fence));
}

void command_ctx_immediate_submit(const CommandContext* cmd_ctx, VkDevice device, VkQueue queue, VkFence fence,
                                  std::function<void(VkCommandBuffer cmd_buf)>&& function) {
    VK_CHECK(vkResetFences(device, 1, &fence));

    command_ctx_begin_primary_buffer(cmd_ctx, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(cmd_ctx->primary_buffer);

    command_ctx_submit_primary_buffer(cmd_ctx, queue, fence, nullptr, nullptr);

    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, vk_opts::timeout_dur));
}
