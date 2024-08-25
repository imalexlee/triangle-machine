#pragma once

#include "core/window.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_frame.h"
#include "vk_backend/vk_utils.h"
#include <core/camera.h>
#include <cstdint>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vk_backend/vk_options.h>
#include <vk_backend/vk_pipeline.h>
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct Stats {
    uint64_t total_draw_time;
    uint64_t total_frame_time;
    uint64_t total_fps;
    float    scene_update_time;
    float    frame_time;
    float    draw_time;
};

struct VkBackend {
    DeviceContext             device_ctx;
    VkInstance                instance;
    VkSurfaceKHR              surface;
    Debugger                  debugger;
    SwapchainContext          swapchain_context;
    VmaAllocator              allocator;
    PipelineInfo              opaque_pipeline_info;
    PipelineInfo              transparent_pipeline_info;
    VkDescriptorSetLayout     global_desc_set_layout;
    VkDescriptorSetLayout     mat_desc_set_layout;
    const Camera*             camera;
    VkDescriptorSetLayout     draw_obj_desc_set_layout;
    Stats                     stats;
    CommandContext            imm_cmd_context;
    VkDescriptorPool          imm_descriptor_pool;
    VkFence                   imm_fence;
    AllocatedImage            color_image;
    AllocatedImage            color_resolve_image;
    VkExtent2D                image_extent;
    VkClearValue              scene_clear_value;
    VkRenderingAttachmentInfo scene_color_attachment;
    VkRenderingAttachmentInfo scene_depth_attachment;
    VkRenderingInfo           scene_rendering_info;
    AllocatedImage            depth_image;
    uint64_t                  frame_num{1};
    std::array<Frame, 3>      frames;
    FrameData                 frame_data;
    VkSampler                 default_linear_sampler;
    VkSampler                 default_nearest_sampler;
    AllocatedImage            default_texture;
    DescriptorAllocator       mat_desc_allocator; // draw object materials
    DescriptorAllocator       obj_desc_allocator; // draw object attributes
    DeletionQueue             deletion_queue;
};

void init_backend(VkBackend* backend, Window* window, const Camera* camera);

void deinit_backend(VkBackend* backend);

void draw(VkBackend* backend, const Entity* entity);

void immediate_submit(const VkBackend*                           backend,
                      std::function<void(VkCommandBuffer cmd)>&& function);
