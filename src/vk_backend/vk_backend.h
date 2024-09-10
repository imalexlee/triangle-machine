#pragma once

#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_frame.h"
#include <vk_backend/vk_pipeline.h>
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_swapchain.h>
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
    DeletionQueue             deletion_queue;
};

VkInstance create_vk_instance(const char* app_name, const char* engine_name);

void init_backend(VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, uint32_t width,
                  uint32_t height);

void finish_pending_vk_work(const VkBackend* backend);

void deinit_backend(VkBackend* backend);

void draw(VkBackend* backend, std::span<const Entity> entities, const SceneData* scene_data);

void immediate_submit(const VkBackend*                           backend,
                      std::function<void(VkCommandBuffer cmd)>&& function);

void create_imgui_vk_resources(VkBackend* backend);

void create_pipeline(VkBackend* backend, const char* vert_shader_path,
                     const char* frag_shader_path);

template <typename T>
[[nodiscard]] MeshBuffers upload_mesh(const VkBackend*                backend,
                                      const std::span<const uint32_t> indices,
                                      std::span<const T>              vertices) {

    const size_t vertex_buffer_bytes = vertices.size() * sizeof(T);
    const size_t index_buffer_bytes  = indices.size() * sizeof(uint32_t);

    AllocatedBuffer staging_buf =
        create_buffer(vertex_buffer_bytes + index_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, vertices.data(), staging_buf.allocation, 0,
                              vertex_buffer_bytes);

    vmaCopyMemoryToAllocation(backend->allocator, indices.data(), staging_buf.allocation,
                              vertex_buffer_bytes, index_buffer_bytes);

    MeshBuffers new_mesh_buffer;
    new_mesh_buffer.vertices =
        // ReSharper disable once CppClassIsIncomplete
        create_buffer(vertex_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0);
    new_mesh_buffer.indices =
        create_buffer(index_buffer_bytes, backend->allocator,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0);

    immediate_submit(backend, [&](const VkCommandBuffer cmd) {
        VkBufferCopy vertex_buffer_region{};
        vertex_buffer_region.size      = vertex_buffer_bytes;
        vertex_buffer_region.srcOffset = 0;
        vertex_buffer_region.dstOffset = 0;

        vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.vertices.buffer, 1,
                        &vertex_buffer_region);

        VkBufferCopy index_buffer_region{};
        index_buffer_region.size      = index_buffer_bytes;
        index_buffer_region.srcOffset = vertex_buffer_bytes;
        index_buffer_region.dstOffset = 0;

        vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.indices.buffer, 1,
                        &index_buffer_region);
    });

    destroy_buffer(backend->allocator, &staging_buf);
    return new_mesh_buffer;
}
