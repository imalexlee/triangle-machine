#pragma once

#include "resources/vk_accel_struct.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_frame.h"
#include "vk_backend/vk_shader.h"
#include "vk_ext.h"
#include <iostream>

#include <vk_backend/vk_pipeline.h>
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_swapchain.h>
// #include <vulkan/vulkan_core.h>

struct Stats {
    uint64_t total_draw_time{};
    uint64_t total_frame_time{};
    uint64_t total_fps{};
    float    scene_update_time{};
    float    frame_time{};
    float    draw_time{};
};

struct ShaderIndices {
    uint16_t geometry_vert{};
    uint16_t geometry_frag{};
    uint16_t grid_vert{};
    uint16_t grid_frag{};
    uint16_t sky_box_vert{};
    uint16_t sky_box_frag{};
};

struct RenderArea {
    glm::vec2 top_left{0};
    // {width, height}
    glm::vec2 scissor_dimensions{0};
};

struct VkBackend {
    VkInstance                     instance;
    VkSurfaceKHR                   surface;
    VkExtent2D                     image_extent;
    VkClearValue                   scene_clear_value;
    VkRenderingAttachmentInfo      scene_color_attachment;
    VkRenderingAttachmentInfo      scene_depth_attachment;
    VkRenderingInfo                scene_rendering_info;
    VkRenderingAttachmentInfo      ui_color_attachment;
    VkRenderingInfo                ui_rendering_info;
    VkDescriptorPool               imm_descriptor_pool;
    std::vector<VkDescriptorSet>   viewport_desc_sets;
    std::vector<AllocatedImage>    viewport_images;
    VkFence                        imm_fence;
    VkSampler                      default_linear_sampler;
    VkSampler                      default_nearest_sampler;
    VkDescriptorSet                graphics_desc_set;
    std::array<VkDescriptorSet, 3> scene_desc_sets{};
    VkDescriptorSetLayout          scene_desc_set_layout;
    VkDescriptorSetLayout          graphics_desc_set_layout;
    VkPipelineLayout               geometry_pipeline_layout;
    VkPipelineLayout               sky_box_pipeline_layout;
    VkPipelineLayout               grid_pipeline_layout;
    VmaAllocator                   allocator{};
    RenderArea                     render_area{};
    DescriptorAllocator            scene_desc_allocator{};
    DescriptorAllocator            graphics_desc_allocator{};
    DeviceContext                  device_ctx{};
    Debugger                       debugger{};
    SwapchainContext               swapchain_context{};
    PipelineInfo                   opaque_pipeline_info{};
    PipelineInfo                   transparent_pipeline_info{};
    ShaderContext                  shader_ctx{};
    ShaderIndices                  shader_indices{};
    Stats                          stats{};
    CommandContext                 immediate_cmd_ctx{};
    CommandContext                 compute_cmd_ctx{};
    AccelStructContext             accel_struct_ctx{};
    AllocatedImage                 color_image{};
    AllocatedImage                 color_resolve_image{};
    AllocatedImage                 depth_image{};
    std::vector<AllocatedImage>    tex_images{};
    uint32_t                       tex_sampler_desc_count{};
    AllocatedBuffer                mat_buffer{};
    uint32_t                       mat_count{0};
    AllocatedImage                 sky_box_image{};
    AllocatedBuffer                sky_box_vert_buffer{};
    std::array<AllocatedBuffer, 3> scene_data_buffers{};
    uint64_t                       frame_num{1};
    uint8_t                        current_frame_i{};
    std::array<Frame, 3>           frames{};
    WorldData                      scene_data{};
    ExtContext                     ext_ctx{};
    DeletionQueue                  deletion_queue{};
    static constexpr uint32_t      material_desc_binding     = 0;
    static constexpr uint32_t      accel_struct_desc_binding = 1;
    static constexpr uint32_t      sky_box_desc_binding      = 2;
    static constexpr uint32_t      texture_desc_binding      = 3;
};

[[nodiscard]] VkInstance vk_instance_create(const char* app_name, const char* engine_name);

void backend_init(VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, uint32_t width, uint32_t height);

void backend_finish_pending_vk_work(const VkBackend* backend);

void backend_deinit(VkBackend* backend);

void backend_draw(VkBackend* backend, std::vector<Entity>& entities, const WorldData* scene_data, size_t vert_shader, size_t frag_shader);

void backend_create_imgui_resources(VkBackend* backend);

void backend_update_render_area(VkBackend* backend, const RenderArea* render_area);

void backend_upload_vert_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name);

void backend_upload_frag_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name);

void backend_upload_sky_box_shaders(VkBackend* backend, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                    const std::string& name);

void backend_upload_sky_box(VkBackend* backend, const uint8_t* texture_data, uint32_t color_channels, uint32_t width, uint32_t height);

void backend_recompile_frag_shader(VkBackend* backend, uint32_t shader_idx);

[[nodiscard]] uint32_t backend_upload_2d_textures(VkBackend* backend, std::vector<TextureSampler>& tex_samplers);

void backend_create_accel_struct(VkBackend* backend, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                 std::span<const TopLevelInstanceRef> instance_refs);

void backend_update_accel_struct(VkBackend* backend, const glm::mat4* transform, uint32_t instance_idx);

template <typename T>
[[nodiscard]] MeshBuffers backend_upload_mesh(VkBackend* backend, const std::span<const uint32_t> indices, std::span<const T> vertices) {

    const size_t vertex_buffer_bytes = vertices.size() * sizeof(T);
    const size_t index_buffer_bytes  = indices.size() * sizeof(uint32_t);

    AllocatedBuffer staging_buf =
        allocated_buffer_create(backend->allocator, vertex_buffer_bytes + index_buffer_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, vertices.data(), staging_buf.allocation, 0, vertex_buffer_bytes);

    vmaCopyMemoryToAllocation(backend->allocator, indices.data(), staging_buf.allocation, vertex_buffer_bytes, index_buffer_bytes);

    MeshBuffers new_mesh_buffer;
    new_mesh_buffer.vertices =
        allocated_buffer_create(backend->allocator, vertex_buffer_bytes,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
    new_mesh_buffer.indices =
        allocated_buffer_create(backend->allocator, index_buffer_bytes,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

    command_ctx_immediate_submit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics,
                                 backend->imm_fence, [&](VkCommandBuffer cmd) {
                                     VkBufferCopy vertex_buffer_region{};
                                     vertex_buffer_region.size      = vertex_buffer_bytes;
                                     vertex_buffer_region.srcOffset = 0;
                                     vertex_buffer_region.dstOffset = 0;

                                     vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.vertices.buffer, 1, &vertex_buffer_region);

                                     VkBufferCopy index_buffer_region{};
                                     index_buffer_region.size      = index_buffer_bytes;
                                     index_buffer_region.srcOffset = vertex_buffer_bytes;
                                     index_buffer_region.dstOffset = 0;

                                     vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.indices.buffer, 1, &index_buffer_region);
                                 });

    allocated_buffer_destroy(backend->allocator, &staging_buf);
    backend->deletion_queue.push_persistent([=] {
        vmaDestroyBuffer(backend->allocator, new_mesh_buffer.indices.buffer, new_mesh_buffer.indices.allocation);
        vmaDestroyBuffer(backend->allocator, new_mesh_buffer.vertices.buffer, new_mesh_buffer.vertices.allocation);
    });
    return new_mesh_buffer;
}

template <typename T> [[nodiscard]] uint32_t backend_upload_materials(VkBackend* backend, std::vector<T>& materials) {
    // save to return later
    const uint32_t curr_mat_offset = backend->mat_count;

    const uint32_t new_material_bytes = materials.size() * sizeof(T);

    AllocatedBuffer staging_buf = allocated_buffer_create(backend->allocator, new_material_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, materials.data(), staging_buf.allocation, 0, new_material_bytes);

    AllocatedBuffer new_mat_buf;
    if (backend->mat_count > 0) {
        // already had previous material data in the buffer

        const uint32_t old_material_bytes = backend->mat_count * sizeof(T);

        new_mat_buf =
            allocated_buffer_create(backend->allocator, new_material_bytes + old_material_bytes,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

        VkBufferCopy old_materials_region{};
        old_materials_region.size      = old_material_bytes;
        old_materials_region.srcOffset = 0;
        old_materials_region.dstOffset = 0;

        VkBufferCopy new_materials_region{};
        new_materials_region.size      = new_material_bytes;
        new_materials_region.srcOffset = 0;
        new_materials_region.dstOffset = old_material_bytes;

        command_ctx_immediate_submit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics,
                                     backend->imm_fence, [&](VkCommandBuffer cmd) {
                                         vkCmdCopyBuffer(cmd, backend->mat_buffer.buffer, new_mat_buf.buffer, 1, &old_materials_region);
                                         vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mat_buf.buffer, 1, &new_materials_region);
                                     });

        allocated_buffer_destroy(backend->allocator, &backend->mat_buffer);
    } else {
        // this is the first material allocation
        new_mat_buf =
            allocated_buffer_create(backend->allocator, new_material_bytes,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

        VkBufferCopy new_materials_region{};
        new_materials_region.size      = new_material_bytes;
        new_materials_region.srcOffset = 0;
        new_materials_region.dstOffset = 0;

        command_ctx_immediate_submit(
            &backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics, backend->imm_fence,
            [&](VkCommandBuffer cmd) { vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mat_buf.buffer, 1, &new_materials_region); });
    }
    allocated_buffer_destroy(backend->allocator, &staging_buf);

    backend->mat_buffer = new_mat_buf;
    backend->mat_count += materials.size();

    // update the material descriptor
    DescriptorWriter descriptor_writer;
    desc_writer_write_buffer_desc(&descriptor_writer, backend->material_desc_binding, backend->mat_buffer.buffer, backend->mat_count * sizeof(T), 0,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    desc_writer_update_desc_set(&descriptor_writer, backend->device_ctx.logical_device, backend->graphics_desc_set);

    return curr_mat_offset;
}