#include "vk_frame.h"
#include "vk_backend/vk_sync.h"

void init_frame(Frame* frame, VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index) {
    frame->render_semaphore  = create_semaphore(device);
    frame->present_semaphore = create_semaphore(device);
    frame->render_fence      = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

    init_cmd_context(&frame->command_context, device, graphics_family_index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
}

void reset_frame_sync(Frame* frame, VkDevice device) {
    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);

    frame->render_semaphore  = create_semaphore(device);
    frame->present_semaphore = create_semaphore(device);
    frame->render_fence      = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);
}

void deinit_frame(const Frame* frame, VkDevice device) {
    DEBUG_PRINT("Destroying Frame");
    deinit_cmd_context(&frame->command_context, device);

    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);
}
