#pragma once

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_command.h"

// contains per-frame shader information
struct SceneData {
    glm::mat4 view{1.f};
    glm::mat4 proj{1.f};
    glm::vec4 cam_pos{};
};

struct Frame {
    CommandContext command_context{};
    VkSemaphore    render_semaphore;
    VkSemaphore    present_semaphore;
    VkFence        render_fence;
};

/**
 * @brief Initializes state like sync structures and buffers for a Frame
 *
 * @param frame			        The frame to init
 * @param device		        The device to acquire resources from
 * @param allocator		        The allocator used to acquire resources
 * @param graphics_family_index	Index of Vulkan queue which supports graphics operations
 */
void frame_init(Frame* frame, VkDevice device, VmaAllocator allocator, uint32_t graphics_family_index);

/**
 * @brief Resets Vulkan synchronization structures for this frame
 *
 * @param frame	  The frame to reset sync for
 * @param device  The device to destroy, and recreate sync structures for
 */
void frame_reset_synchronization(Frame* frame, VkDevice device);

/**
 * @brief Destroys Vulkan sync structures, command pools, and descriptor pools for this Frame
 *
 * @param frame	  The frame to deinitialize
 * @param device  The device associated with the memory to be destroyed
 */
void frame_deinit(const Frame* frame, VkDevice device);
