#pragma once

#include "resources/vk_shader.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_frame.h"
#include "vk_ext.h"
#include <vk_backend/vk_pipeline.h>
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_swapchain.h>
// #include <vulkan/vulkan_core.h>

struct Stats {
    uint64_t total_draw_time;
    uint64_t total_frame_time;
    uint64_t total_fps;
    float    scene_update_time;
    float    frame_time;
    float    draw_time;
};

struct VkBackend {
    VkInstance                instance;
    VkSurfaceKHR              surface;
    VkExtent2D                image_extent;
    VkClearValue              scene_clear_value;
    VkRenderingAttachmentInfo scene_color_attachment;
    VkRenderingAttachmentInfo scene_depth_attachment;
    VkRenderingInfo           scene_rendering_info;
    VkDescriptorPool          imm_descriptor_pool;
    VkFence                   imm_fence;
    VkSampler                 default_linear_sampler;
    VkSampler                 default_nearest_sampler;
    VkPipelineLayout          geo_pipeline_layout;
    VkPipelineLayout          sky_box_pipeline_layout;
    VkDescriptorSet           sky_box_desc_set;
    VkDescriptorSetLayout     sky_box_desc_set_layout;
    VkDescriptorSetLayout     global_desc_set_layout;
    VkDescriptorSetLayout     mat_desc_set_layout;
    VkDescriptorSetLayout     draw_obj_desc_set_layout;
    VmaAllocator              allocator;
    DescriptorAllocator       sky_box_desc_allocator;
    DeviceContext             device_ctx;
    Debugger                  debugger;
    SwapchainContext          swapchain_context;
    PipelineInfo              opaque_pipeline_info;
    PipelineInfo              transparent_pipeline_info;
    PipelineInfo              grid_pipeline_info;
    ShaderContext             shader_ctx;
    Stats                     stats;
    CommandContext            immediate_cmd_ctx;
    CommandContext            compute_cmd_ctx;
    AllocatedImage            color_image;
    AllocatedImage            color_resolve_image;
    AllocatedImage            depth_image;
    AllocatedImage            default_texture;
    AllocatedBuffer           sky_box_buffer;
    uint64_t                  frame_num{1};
    std::array<Frame, 3>      frames;
    SceneData                 scene_data;
    VkExtContext              ext_ctx;
    DeletionQueue             deletion_queue;
};

VkInstance create_vk_instance(const char* app_name, const char* engine_name);

void init_backend(VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, uint32_t width,
                  uint32_t height);

void finish_pending_vk_work(const VkBackend* backend);

void deinit_backend(VkBackend* backend);

void draw(VkBackend* backend, std::span<const Entity> entities, const SceneData* scene_data,
          size_t vert_shader, size_t frag_shader);

void immediate_submit(const VkBackend*                           backend,
                      std::function<void(VkCommandBuffer cmd)>&& function);

void create_imgui_vk_resources(VkBackend* backend);

void create_pipeline(VkBackend* backend, std::span<uint32_t> vert_shader_spv,
                     std::span<uint32_t> frag_shader_spv);

void upload_vert_shader(VkBackend* backend, const std::filesystem::path& file_path,
                        const std::string& name);

void upload_frag_shader(VkBackend* backend, const std::filesystem::path& file_path,
                        const std::string& name);

void upload_sky_box_shaders(VkBackend* backend, const std::filesystem::path& vert_path,
                            const std::filesystem::path& frag_path, const std::string& name);

void upload_sky_box(VkBackend* backend, const uint8_t* texture_data, uint32_t color_channels,
                    uint32_t width, uint32_t height);

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
