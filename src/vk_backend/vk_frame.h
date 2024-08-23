#pragma once

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_command.h"
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

// contains per-frame shader information
struct FrameData {
    glm::mat4 view_proj{1.f};
    glm::vec3 eye_pos;
};

struct Frame {
    CommandContext command_context;
    VkSemaphore render_semaphore;
    VkSemaphore present_semaphore;
    VkFence render_fence;
    AllocatedBuffer frame_data_buf;
    VkDescriptorSet desc_set;
    DescriptorAllocator desc_allocator;
};

/**
 * @brief Initializes state like sync structures and buffers for a Frame
 *
 * @param frame			  The frame to init
 * @param device		  The device to acquire resources from
 * @param allocator		  The allocator used to acquire resources
 * @param graphics_family_index	  Index of Vulkan queue which supports graphics operations
 * @param set_layout		  Descriptor set layout for resources attached to this frame
 */
void init_frame(Frame* frame, VkDevice device, VmaAllocator allocator,
                uint32_t graphics_family_index, VkDescriptorSetLayout set_layout);

/**
 * @brief Writes new scene data to device-local memory to be used during this frames presentation
 *
 * @param frame	      The frame to write this scene data to
 * @param device      The device associated with the allocated memory
 * @param allocator   The allocator used to allocate this memory initially
 * @param scene_data  The scene data
 */
void set_frame_data(const Frame* frame, VkDevice device, VmaAllocator allocator,
                    const FrameData* frame_data);

/**
 * @brief Resets Vulkan synchronization structures for this frame
 *
 * @param frame	  The frame to reset sync for
 * @param device  The device to destroy, and recreate sync structures for
 */
void reset_frame_sync(Frame* frame, VkDevice device);

/**
 * @brief Destroys Vulkan sync structures, command pools, and descriptor pools for this Frame
 *
 * @param frame	  The frame to deinitialize
 * @param device  The device associated with the memory to be destroyed
 */
void deinit_frame(Frame* frame, VkDevice device);
