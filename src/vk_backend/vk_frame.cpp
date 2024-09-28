#include "vk_frame.h"
#include "vk_backend/vk_sync.h"

void frame_init(Frame* frame, VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index) {
    frame->render_semaphore  = vk_semaphore_create(device);
    frame->present_semaphore = vk_semaphore_create(device);
    frame->render_fence      = vk_fence_create(device, VK_FENCE_CREATE_SIGNALED_BIT);

    command_ctx_init(&frame->command_context, device, graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void frame_reset_synchronization(Frame* frame, VkDevice device) {
    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);

    frame->render_semaphore  = vk_semaphore_create(device);
    frame->present_semaphore = vk_semaphore_create(device);
    frame->render_fence      = vk_fence_create(device, VK_FENCE_CREATE_SIGNALED_BIT);
}

void frame_deinit(const Frame* frame, VkDevice device) {
    DEBUG_PRINT("Destroying Frame");
    command_ctx_deinit(&frame->command_context, device);

    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);
}
