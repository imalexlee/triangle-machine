#include "vk_frame.h"
#include "vk_backend/vk_sync.h"
#include <array>

void init_frame(Frame* frame, VkDevice device, VmaAllocator allocator,
                uint32_t graphics_family_index, VkDescriptorSetLayout set_layout) {
    frame->render_semaphore  = create_semaphore(device);
    frame->present_semaphore = create_semaphore(device);
    frame->render_fence      = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);

    init_cmd_context(&frame->command_context, device, graphics_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    frame->frame_data_buf = create_buffer(
        sizeof(SceneData), allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    std::array<PoolSizeRatio, 1> pool_sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}}};

    init_desc_allocator(&frame->desc_allocator, device, 1, pool_sizes);

    frame->desc_set = allocate_desc_set(&frame->desc_allocator, device, set_layout);
}

void set_scene_data(const Frame* frame, VkDevice device, VmaAllocator allocator,
                    const SceneData* scene_data) {
    vmaCopyMemoryToAllocation(allocator, scene_data, frame->frame_data_buf.allocation, 0,
                              sizeof(SceneData));

    DescriptorWriter desc_writer;
    write_buffer_desc(&desc_writer, 0, frame->frame_data_buf.buffer, sizeof(SceneData), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    update_desc_set(&desc_writer, device, frame->desc_set);
}

void reset_frame_sync(Frame* frame, VkDevice device) {
    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);

    frame->render_semaphore  = create_semaphore(device);
    frame->present_semaphore = create_semaphore(device);
    frame->render_fence      = create_fence(device, VK_FENCE_CREATE_SIGNALED_BIT);
}

void deinit_frame(Frame* frame, VkDevice device) {
    DEBUG_PRINT("Destroying Frame");
    deinit_cmd_context(&frame->command_context, device);
    deinit_desc_allocator(&frame->desc_allocator, device);

    vkDestroySemaphore(device, frame->render_semaphore, nullptr);
    vkDestroySemaphore(device, frame->present_semaphore, nullptr);
    vkDestroyFence(device, frame->render_fence, nullptr);
}
