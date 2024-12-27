#pragma once

#include "system/device/memory/vk_accel_struct.h"
#include "system/device/memory/vk_descriptor.h"
#include "system/device/memory/vk_image.h"
#include "system/device/vk_command.h"
#include "system/device/vk_context.h"
#include "system/device/vk_ext.h"
#include "system/device/vk_types.h"
#include "vk_frame.h"
#include "vk_shader.h"
#include <scene/scene.h>

#include "vk_pipeline.h"
#include "vk_swapchain.h"
#include <system/device/vk_utils.h>

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
    uint16_t cursor_vert{};
    uint16_t cursor_frag{};
};

struct RenderArea {
    glm::vec2 top_left{0};
    glm::vec2 scissor_dimensions{0}; // {width, height}
};

struct Renderer {
    const VkContext*               vk_ctx;
    VkExtent2D                     image_extent;
    VkClearValue                   scene_clear_value;
    VkRenderingAttachmentInfo      scene_color_attachment;
    VkRenderingAttachmentInfo      scene_entity_id_attachment;
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
    std::vector<VkSampler>         mip_mapped_samplers;
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
    SwapchainContext               swapchain_context{};
    PipelineInfo                   opaque_pipeline_info{};
    PipelineInfo                   transparent_pipeline_info{};
    ShaderContext                  shader_ctx{};
    ShaderIndices                  shader_indices{};
    Stats                          stats{};
    CommandContext                 immediate_cmd_ctx{};
    CommandContext                 compute_cmd_ctx{};
    AccelStructContext             accel_struct_ctx{};
    AllocatedImage                 color_msaa_image{};
    AllocatedImage                 color_image{};
    AllocatedImage                 depth_image{};
    std::vector<AllocatedImage>    tex_images{};
    uint32_t                       tex_sampler_desc_count{};
    AllocatedBuffer                mat_buffer{};
    std::array<AllocatedImage, 3>  entity_id_images{};
    uint32_t                       mat_count{};
    AllocatedImage                 sky_box_image{};
    AllocatedBuffer                sky_box_vert_buffer{};
    AllocatedBuffer                entity_id_result_buffer;
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
    uint32_t                       msaa_samples{1};

    // I hate this so much. only temporary. editor should be completely seperated from renderer and engine
    enum class EngineMode mode {};
};

void renderer_init(Renderer* renderer, const VkContext* vk_ctx, uint32_t width, uint32_t height, EngineMode mode);

void renderer_finish_pending_vk_work(const Renderer* renderer);

[[nodiscard]] uint16_t renderer_entity_id_at_pos(const Renderer* renderer, int32_t x, int32_t y);

void renderer_deinit(Renderer* renderer);

void renderer_draw(Renderer* renderer, std::vector<Entity>& entities, const WorldData* scene_data, enum class EngineFeatures engine_features);

void renderer_create_imgui_resources(Renderer* renderer);

void renderer_update_render_area(Renderer* renderer, const RenderArea* render_area);

void renderer_upload_vert_shader(Renderer* renderer, const std::filesystem::path& file_path, const std::string& name);

void renderer_upload_frag_shader(Renderer* renderer, const std::filesystem::path& file_path, const std::string& name);

void renderer_upload_sky_box_shaders(Renderer* renderer, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                     const std::string& name);

void renderer_upload_cursor_shaders(Renderer* renderer);

void renderer_upload_sky_box(Renderer* renderer, const uint8_t* texture_data, uint32_t color_channels, uint32_t width, uint32_t height);

void renderer_recompile_frag_shader(Renderer* renderer, uint32_t shader_idx);

[[nodiscard]] uint32_t renderer_upload_2d_textures(Renderer* renderer, std::vector<TextureSampler>& tex_samplers);

void renderer_create_accel_struct(Renderer* renderer, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                  std::span<const TopLevelInstanceRef> instance_refs);

void renderer_update_accel_struct(Renderer* renderer, const glm::mat4* transform, uint32_t instance_idx);

template <typename T>
[[nodiscard]] MeshBuffers renderer_upload_mesh(Renderer* renderer, const std::span<const uint32_t> indices, std::span<const T> vertices) {

    const size_t vertex_buffer_bytes = vertices.size() * sizeof(T);
    const size_t index_buffer_bytes  = indices.size() * sizeof(uint32_t);

    AllocatedBuffer staging_buf =
        allocated_buffer_create(renderer->allocator, vertex_buffer_bytes + index_buffer_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(renderer->allocator, vertices.data(), staging_buf.allocation, 0, vertex_buffer_bytes);

    vmaCopyMemoryToAllocation(renderer->allocator, indices.data(), staging_buf.allocation, vertex_buffer_bytes, index_buffer_bytes);

    MeshBuffers new_mesh_buffer;
    new_mesh_buffer.vertices =
        allocated_buffer_create(renderer->allocator, vertex_buffer_bytes,
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
    new_mesh_buffer.indices =
        allocated_buffer_create(renderer->allocator, index_buffer_bytes,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

    command_ctx_immediate_submit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                 renderer->imm_fence, [&](VkCommandBuffer cmd) {
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

    allocated_buffer_destroy(renderer->allocator, &staging_buf);
    renderer->deletion_queue.push_persistent([=] {
        vmaDestroyBuffer(renderer->allocator, new_mesh_buffer.indices.buffer, new_mesh_buffer.indices.allocation);
        vmaDestroyBuffer(renderer->allocator, new_mesh_buffer.vertices.buffer, new_mesh_buffer.vertices.allocation);
    });
    return new_mesh_buffer;
}

template <typename T> [[nodiscard]] uint32_t renderer_upload_materials(Renderer* renderer, std::vector<T>& materials) {
    // save to return later
    const uint32_t curr_mat_offset = renderer->mat_count;

    const uint32_t new_material_bytes = materials.size() * sizeof(T);

    AllocatedBuffer staging_buf = allocated_buffer_create(renderer->allocator, new_material_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(renderer->allocator, materials.data(), staging_buf.allocation, 0, new_material_bytes);

    AllocatedBuffer new_mat_buf;
    if (renderer->mat_count > 0) {
        // already had previous material data in the buffer

        const uint32_t old_material_bytes = renderer->mat_count * sizeof(T);

        new_mat_buf =
            allocated_buffer_create(renderer->allocator, new_material_bytes + old_material_bytes,
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

        command_ctx_immediate_submit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                     renderer->imm_fence, [&](VkCommandBuffer cmd) {
                                         vkCmdCopyBuffer(cmd, renderer->mat_buffer.buffer, new_mat_buf.buffer, 1, &old_materials_region);
                                         vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mat_buf.buffer, 1, &new_materials_region);
                                     });

        allocated_buffer_destroy(renderer->allocator, &renderer->mat_buffer);
    } else {
        // this is the first material allocation
        new_mat_buf =
            allocated_buffer_create(renderer->allocator, new_material_bytes,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);

        VkBufferCopy new_materials_region{};
        new_materials_region.size      = new_material_bytes;
        new_materials_region.srcOffset = 0;
        new_materials_region.dstOffset = 0;

        command_ctx_immediate_submit(
            &renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics, renderer->imm_fence,
            [&](VkCommandBuffer cmd) { vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mat_buf.buffer, 1, &new_materials_region); });
    }
    allocated_buffer_destroy(renderer->allocator, &staging_buf);

    renderer->mat_buffer = new_mat_buf;
    renderer->mat_count += materials.size();

    // update the material descriptor
    DescriptorWriter descriptor_writer;
    desc_writer_write_buffer_desc(&descriptor_writer, renderer->material_desc_binding, renderer->mat_buffer.buffer, renderer->mat_count * sizeof(T),
                                  0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    desc_writer_update_desc_set(&descriptor_writer, renderer->vk_ctx->logical_device, renderer->graphics_desc_set);

    return curr_mat_offset;
}