#pragma once

#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_frame.h"
#include "vk_backend/vk_utils.h"
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
    SceneData                 scene_data;
    VkSampler                 default_linear_sampler;
    VkSampler                 default_nearest_sampler;
    AllocatedImage            default_texture;
    DescriptorAllocator       mat_desc_allocator; // draw object materials
    DescriptorAllocator       obj_desc_allocator; // draw object attributes
    DeletionQueue             deletion_queue;
};

VkInstance create_vk_instance(const char* app_name, const char* engine_name);

void init_backend(
    VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, int width, int height);

void finish_pending_vk_work(VkBackend* backend);

void deinit_backend(VkBackend* backend);

void draw(VkBackend* backend, const std::span<Entity> entities, const SceneData* scene_data);

void immediate_submit(const VkBackend*                           backend,
                      std::function<void(VkCommandBuffer cmd)>&& function);

void create_imgui_vk_resources(VkBackend* backend);

void create_pipeline(VkBackend*  backend,
                     const char* vert_shader_path,
                     const char* frag_shader_path);
