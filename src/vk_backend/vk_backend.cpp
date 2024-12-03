
#include <array>
#include <iostream>
#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "global_utils.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_backend.h"
#include "vk_backend/vk_pipeline.h"
#include "vk_backend/vk_sync.h"
#include "vk_init.h"
#include "vk_options.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstring>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fstream>
#include <future>
#include <nvtt/nvtt.h>
#include <taskflow/taskflow/taskflow.hpp>
#include <vk_backend/vk_command.h>
#include <vk_backend/vk_debug.h>
#include <vk_backend/vk_device.h>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>
#include <vk_backend/vk_utils.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <core/engine.h>

// initialization
static void create_allocator(VkBackend* backend);
static void create_default_data(VkBackend* backend);
static void create_desc_layouts(VkBackend* backend);
static void upload_sky_box_texture(VkBackend* backend, const TextureSampler* tex_sampler);
// static void create_desc_allocators(VkBackend* backend);
static void create_graphics_desc_set(VkBackend* backend);
static void create_scene_desc_sets(VkBackend* backend);
static void create_grid_pipeline(VkBackend* backend);
static void create_pipeline_layouts(VkBackend* backend);
static void create_sky_box(VkBackend* backend);
static void create_compute_resources(VkBackend* backend);
static void configure_debugger(VkBackend* backend);
static void configure_render_resources(VkBackend* backend);

static VkShaderModule load_shader_module(const VkBackend* backend, std::span<uint32_t> shader_spv);
// state update
static void resize(VkBackend* backend);
static void set_render_state(VkBackend* backend, VkCommandBuffer cmd_buf);
static void set_scene_data(const VkBackend* backend, const WorldData* scene_data);
// rendering
void        render_geometry(VkBackend* backend, VkCommandBuffer cmd_buf, std::vector<Entity>& entities);
static void render_ui(VkCommandBuffer cmd_buf);
static void render_grid(const VkBackend* backend, VkCommandBuffer cmd_buf);
static void render_sky_box(const VkBackend* backend, VkCommandBuffer cmd_buf);
static void backend_upload_grid_shaders(VkBackend* backend, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                        const std::string& name);

static std::vector<const char*> get_instance_extensions();
static void                     update_curr_frame_idx(VkBackend* backend) { backend->current_frame_i = backend->frame_num % backend->frames.size(); }

using namespace std::chrono;

static VkBackend* active_backend = nullptr;

void backend_init(VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, uint32_t width, uint32_t height, EngineMode mode) {

    assert(active_backend == nullptr);
    active_backend = backend;

    backend->instance = instance;

    device_ctx_init(&backend->device_ctx, backend->instance, surface);
    swapchain_ctx_init(&backend->swapchain_context, &backend->device_ctx, surface, vk_opts::desired_present_mode);

    create_allocator(backend);
    create_desc_layouts(backend);
    create_graphics_desc_set(backend);
    create_scene_desc_sets(backend);

    create_sky_box(backend);

    backend->image_extent.width  = width;
    backend->image_extent.height = height;

    for (size_t i = 0; i < backend->frames.size(); i++) {
        Frame* frame = &backend->frames[i];
        frame_init(frame, backend->device_ctx.logical_device, backend->allocator, backend->device_ctx.queues.graphics_family_index);
        backend->scene_data_buffers[i] =
            allocated_buffer_create(backend->allocator, sizeof(WorldData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        backend->entity_id_images[i] =
            allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16_UINT, 1, 1, VMA_MEMORY_USAGE_AUTO);

        DescriptorWriter writer{};
        desc_writer_write_image_desc(&writer, 1, backend->entity_id_images[i].image_view, backend->default_linear_sampler, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        desc_writer_update_desc_set(&writer, backend->device_ctx.logical_device, backend->scene_desc_sets[i]);
    }

    backend->entity_id_result_buffer =
        allocated_buffer_create(backend->allocator, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

    RenderArea initial_render_area{};
    initial_render_area.scissor_dimensions = glm::vec2{width, height};

    backend->render_area = initial_render_area;

    backend->color_resolve_image = allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                          VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);

    backend->color_image = allocated_image_create(
        backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, backend->device_ctx.raster_samples);

    backend->depth_image =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_D32_SFLOAT, 1, backend->device_ctx.raster_samples);

    backend->mode = mode;
    // TODO: don't worry about viewport images in released scenes. only for editor/debugging
    if (backend->mode == EngineMode::EDIT) {
        for (size_t i = 0; i < backend->swapchain_context.images.size(); i++) {
            AllocatedImage new_viewport_image = allocated_image_create(
                backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);
            backend->viewport_images.push_back(new_viewport_image);
        }
    }

    // create color attachments and  rendering information from our allocated images
    configure_render_resources(backend);

    backend->imm_fence = vk_fence_create(backend->device_ctx.logical_device, VK_FENCE_CREATE_SIGNALED_BIT);

    command_ctx_init(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    command_ctx_init(&backend->compute_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.compute_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    accel_struct_ctx_init(&backend->accel_struct_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics_family_index);

    create_pipeline_layouts(backend);

    create_compute_resources(backend);

    create_default_data(backend);

    shader_ctx_init(&backend->shader_ctx);

    ext_context_init(&backend->ext_ctx, backend->device_ctx.logical_device);

    create_grid_pipeline(backend);

    if constexpr (vk_opts::validation_enabled) {
        configure_debugger(backend);
    }
}

static void create_compute_resources(VkBackend* backend) {
    // TODO: create resources
}

uint16_t backend_entity_id_at_pos(const VkBackend* backend, int32_t x, int32_t y) {

    VkImageSubresourceLayers layers{};
    layers.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    layers.baseArrayLayer = 0;
    layers.layerCount     = 1;
    layers.mipLevel       = 0;

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferImageHeight = 1;
    region.bufferRowLength   = 1;
    region.imageExtent       = {.depth = 1, .height = 1, .width = 1};
    region.imageOffset       = {.x = x, .y = y, .z = 0};
    region.imageSubresource  = layers;

    command_ctx_immediate_submit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics,
                                 backend->imm_fence, [&](VkCommandBuffer cmd) {
                                     vkCmdCopyImageToBuffer(cmd, backend->entity_id_images[backend->current_frame_i].image, VK_IMAGE_LAYOUT_GENERAL,
                                                            backend->entity_id_result_buffer.buffer, 1, &region);
                                 });

    return (*static_cast<uint32_t*>(backend->entity_id_result_buffer.info.pMappedData));
}

void set_scene_data(const VkBackend* backend, const WorldData* scene_data) {
    assert(scene_data);
    assert(backend->current_frame_i < backend->scene_data_buffers.size());
    VkDescriptorSet        curr_scene_desc_set = backend->scene_desc_sets[backend->current_frame_i];
    const AllocatedBuffer* curr_scene_buf      = &backend->scene_data_buffers[backend->current_frame_i];

    VK_CHECK(vmaCopyMemoryToAllocation(backend->allocator, scene_data, curr_scene_buf->allocation, 0, sizeof(WorldData)));

    DescriptorWriter desc_writer{};
    desc_writer_write_buffer_desc(&desc_writer, 0, curr_scene_buf->buffer, sizeof(WorldData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    desc_writer_update_desc_set(&desc_writer, backend->device_ctx.logical_device, curr_scene_desc_set);
}

static constexpr std::array skybox_vertices = {
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
};

void create_sky_box(VkBackend* backend) {
    backend->sky_box_vert_buffer =
        allocated_buffer_create(backend->allocator, 36 * sizeof(glm::vec3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, skybox_vertices.data(), backend->sky_box_vert_buffer.allocation, 0,
                              backend->sky_box_vert_buffer.info.size);

    backend->deletion_queue.push_persistent([=] { allocated_buffer_destroy(backend->allocator, &backend->sky_box_vert_buffer); });
};

void backend_upload_sky_box(VkBackend* backend, const uint8_t* texture_data, uint32_t color_channels, uint32_t width, uint32_t height) {
    TextureSampler sky_box_tex_sampler{};
    sky_box_tex_sampler.sampler   = backend->default_linear_sampler;
    sky_box_tex_sampler.view_type = VK_IMAGE_VIEW_TYPE_CUBE;

    sky_box_tex_sampler.width          = width;
    sky_box_tex_sampler.height         = height;
    sky_box_tex_sampler.color_channels = color_channels;
    sky_box_tex_sampler.layer_count    = 6;

    MipLevel mip_level{};
    mip_level.data   = (void*)texture_data;
    mip_level.height = height;
    mip_level.width  = width;

    sky_box_tex_sampler.mip_levels.push_back(mip_level);

    upload_sky_box_texture(backend, &sky_box_tex_sampler);
}

void create_pipeline_layouts(VkBackend* backend) {

    std::array set_layouts = {backend->scene_desc_set_layout, backend->graphics_desc_set_layout};

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    backend->geometry_pipeline_layout = vk_pipeline_layout_create(backend->device_ctx.logical_device, set_layouts, push_constant_ranges, 0);

    backend->sky_box_pipeline_layout = vk_pipeline_layout_create(backend->device_ctx.logical_device, set_layouts, {}, 0);
}

static void create_grid_pipeline(VkBackend* backend) {

    backend_upload_grid_shaders(backend, "../shaders/vertex/grid.vert", "../shaders/fragment/grid.frag", "grid shaders");

    std::array set_layouts{backend->scene_desc_set_layout};

    // TODO: dont hardcode these
    backend->shader_indices.grid_vert = 0;
    backend->shader_indices.grid_frag = 0;

    backend->grid_pipeline_layout = vk_pipeline_layout_create(backend->device_ctx.logical_device, set_layouts, {}, 0);
}

// adding names to these 64 bit handles helps a lot when reading validation errors
void configure_debugger(VkBackend* backend) {
    debugger_init(&backend->debugger, backend->instance, backend->device_ctx.logical_device);
    debugger_set_handle_name(&backend->debugger, backend->color_image.image, VK_OBJECT_TYPE_IMAGE, "color image");
    debugger_set_handle_name(&backend->debugger, backend->color_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color image view");
    debugger_set_handle_name(&backend->debugger, backend->depth_image.image, VK_OBJECT_TYPE_IMAGE, "depth image");
    debugger_set_handle_name(&backend->debugger, backend->depth_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "depth image view");
    debugger_set_handle_name(&backend->debugger, backend->color_resolve_image.image, VK_OBJECT_TYPE_IMAGE, "color resolve image");
    debugger_set_handle_name(&backend->debugger, backend->color_resolve_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color resolve image view");
    debugger_set_handle_name(&backend->debugger, backend->immediate_cmd_ctx.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "imm cmd_buf buf");

    for (size_t i = 0; i < backend->frames.size(); i++) {
        const Frame& frame = backend->frames[i];
        debugger_set_handle_name(&backend->debugger, frame.command_context.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                 "frame " + std::to_string(i) + " cmd_buf buf");
        debugger_set_handle_name(&backend->debugger, backend->entity_id_images[i].image, VK_OBJECT_TYPE_IMAGE,
                                 "entity id image " + std::to_string(i));
        debugger_set_handle_name(&backend->debugger, backend->entity_id_images[i].image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                                 "entity id image view " + std::to_string(i));
    }

    for (size_t i = 0; i < backend->swapchain_context.images.size(); i++) {
        debugger_set_handle_name(&backend->debugger, backend->swapchain_context.images[i], VK_OBJECT_TYPE_IMAGE,
                                 "swapchain image " + std::to_string(i));
        debugger_set_handle_name(&backend->debugger, backend->swapchain_context.image_views[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                                 "swapchain image view " + std::to_string(i));
    }
}

void create_desc_layouts(VkBackend* backend) {
    DescriptorLayoutBuilder layout_builder;
    // scene data
    desc_layout_builder_add_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // entity id data
    desc_layout_builder_add_binding(&layout_builder, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    backend->scene_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, backend->device_ctx.logical_device);

    desc_layout_builder_clear(&layout_builder);

    // materials
    desc_layout_builder_add_binding(&layout_builder, backend->material_desc_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // acceleration structure
    desc_layout_builder_add_binding(&layout_builder, backend->accel_struct_desc_binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // sky box
    desc_layout_builder_add_binding(&layout_builder, backend->sky_box_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // textures
    constexpr VkDescriptorBindingFlags texture_binding_flags =
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    desc_layout_builder_add_binding(&layout_builder, backend->texture_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, texture_binding_flags, 300);

    backend->graphics_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, backend->device_ctx.logical_device);
}

// creates a scene desc set per frame
void create_scene_desc_sets(VkBackend* backend) {
    std::array<PoolSizeRatio, 2> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3}},
    };
    desc_allocator_init(&backend->scene_desc_allocator, backend->device_ctx.logical_device, 3, pool_sizes);

    for (size_t i = 0; i < backend->scene_desc_sets.size(); i++) {
        backend->scene_desc_sets[i] =
            desc_allocator_allocate_desc_set(&backend->scene_desc_allocator, backend->device_ctx.logical_device, backend->scene_desc_set_layout);
    }
}

void create_graphics_desc_set(VkBackend* backend) {
    std::array<PoolSizeRatio, 4> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
         {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 300}}
    };
    desc_allocator_init(&backend->graphics_desc_allocator, backend->device_ctx.logical_device, 1, pool_sizes);

    backend->graphics_desc_set = desc_allocator_allocate_desc_set(&backend->graphics_desc_allocator, backend->device_ctx.logical_device,
                                                                  backend->graphics_desc_set_layout, 300);
}

void configure_render_resources(VkBackend* backend) {

    VkClearValue scene_clear_value = {.color = {{0.2f, 0.2f, 0.2f, 0.2f}}};

    backend->scene_color_attachment =
        vk_color_attachment_info_create(backend->color_image.image_view, &scene_clear_value, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                        VK_ATTACHMENT_STORE_OP_STORE, backend->color_resolve_image.image_view);

    backend->scene_depth_attachment =
        vk_depth_attachment_info_create(backend->depth_image.image_view, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

    std::array scene_color_attachments = {&backend->scene_color_attachment};
    backend->scene_rendering_info      = vk_rendering_info_create(scene_color_attachments, &backend->scene_depth_attachment, backend->image_extent);

    backend->ui_color_attachment =
        vk_color_attachment_info_create(backend->color_resolve_image.image_view, nullptr, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

    std::array ui_color_attachments = {&backend->ui_color_attachment};
    backend->ui_rendering_info      = vk_rendering_info_create(ui_color_attachments, nullptr, backend->image_extent);
}

void create_default_data(VkBackend* backend) {

    backend->stats.total_fps        = 0;
    backend->stats.total_frame_time = 0;
    backend->stats.total_draw_time  = 0;

    backend->default_linear_sampler = vk_sampler_create(backend->device_ctx.logical_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR);

    backend->default_nearest_sampler = vk_sampler_create(backend->device_ctx.logical_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

    uint32_t white_data[16];
    for (size_t i = 0; i < 16; i++) {
        white_data[i] = 0xFFFFFFFF;
    }
    TextureSampler default_tex_sampler{};
    default_tex_sampler.width          = 4;
    default_tex_sampler.height         = 4;
    default_tex_sampler.color_channels = 4;
    default_tex_sampler.layer_count    = 1;
    default_tex_sampler.sampler        = backend->default_linear_sampler;
    default_tex_sampler.view_type      = VK_IMAGE_VIEW_TYPE_2D;

    MipLevel new_mip_level{};
    new_mip_level.data   = &white_data;
    new_mip_level.height = 4;
    new_mip_level.width  = 4;

    default_tex_sampler.mip_levels.push_back(new_mip_level);

    // default_tex_sampler.data      = reinterpret_cast<const uint8_t*>(&white_data);
    // default_tex_sampler.byte_size = 4 * 4 * 4;

    std::vector<TextureSampler> tex_samplers = {default_tex_sampler};

    // default texture will always be assumed to be at index 0
    std::ignore = backend_upload_2d_textures(backend, tex_samplers, VK_FORMAT_R8G8B8A8_SRGB);
}

void backend_create_imgui_resources(VkBackend* backend) {

    assert(backend->mode == EngineMode::EDIT);

    std::array<VkDescriptorPoolSize, 11> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}}
    };

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.maxSets       = 1000;
    pool_ci.pPoolSizes    = pool_sizes.data();
    pool_ci.poolSizeCount = pool_sizes.size();
    pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(backend->device_ctx.logical_device, &pool_ci, nullptr, &backend->imm_descriptor_pool));

    VkPipelineRenderingCreateInfoKHR pipeline_info{};
    pipeline_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_info.pColorAttachmentFormats = &backend->color_resolve_image.image_format;
    pipeline_info.colorAttachmentCount    = 1;
    pipeline_info.depthAttachmentFormat   = backend->depth_image.image_format;

    ImGui_ImplVulkan_InitInfo init_info   = {};
    init_info.Instance                    = backend->instance;
    init_info.PhysicalDevice              = backend->device_ctx.physical_device;
    init_info.Device                      = backend->device_ctx.logical_device;
    init_info.Queue                       = backend->device_ctx.queues.graphics;
    init_info.DescriptorPool              = backend->imm_descriptor_pool;
    init_info.MinImageCount               = 3;
    init_info.ImageCount                  = 3;
    init_info.UseDynamicRendering         = true;
    init_info.PipelineRenderingCreateInfo = pipeline_info;
    init_info.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    for (size_t i = 0; i < backend->viewport_images.size(); i++) {
        VkDescriptorSet new_viewport_ds = ImGui_ImplVulkan_AddTexture(backend->default_linear_sampler, backend->viewport_images[i].image_view,
                                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        backend->viewport_desc_sets.push_back(new_viewport_ds);
    }
}
void backend_update_render_area(VkBackend* backend, const RenderArea* render_area) { backend->render_area = *render_area; }

void create_allocator(VkBackend* backend) {
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.device         = backend->device_ctx.logical_device;
    allocator_info.physicalDevice = backend->device_ctx.physical_device;
    allocator_info.instance       = backend->instance;
    allocator_info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocator_info, &backend->allocator));
}

VkInstance vk_instance_create(const char* app_name, const char* engine_name) {

    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext              = nullptr;
    app_info.pApplicationName   = app_name;
    app_info.pEngineName        = engine_name;
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    const std::vector<const char*> instance_extensions = get_instance_extensions();

    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo        = &app_info;
    instance_ci.flags                   = 0;
    instance_ci.ppEnabledExtensionNames = instance_extensions.data();
    instance_ci.enabledExtensionCount   = instance_extensions.size();

    VkDebugUtilsMessengerCreateInfoEXT debug_ci;
    VkValidationFeaturesEXT            validation_features;
    std::array<const char*, 1>         validation_layers;

    if constexpr (vk_opts::validation_enabled) {
        debug_ci            = vk_messenger_info_create();
        validation_features = vk_validation_features_create();
        validation_layers   = vk_validation_layers_create();

        validation_features.pNext       = &debug_ci;
        instance_ci.pNext               = &validation_features;
        instance_ci.enabledLayerCount   = validation_layers.size();
        instance_ci.ppEnabledLayerNames = validation_layers.data();
    }

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &instance));
    return instance;
}

void backend_recompile_frag_shader(VkBackend* backend, uint32_t shader_idx) {
    assert(shader_idx < backend->shader_ctx.frag_shaders.size());
    VK_CHECK(vkDeviceWaitIdle(backend->device_ctx.logical_device));

    DEBUG_PRINT("recompiling shaders\n");
    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };
    Shader* shader = &backend->shader_ctx.frag_shaders[shader_idx];

    shader_ctx_stage_shader(&backend->shader_ctx, shader->path, shader->name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_replace_shader(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked, shader_idx);
}

void backend_upload_grid_shaders(VkBackend* backend, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                 const std::string& name) {

    std::array set_layouts{
        backend->scene_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, vert_path, name, set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&backend->shader_ctx, frag_path, name, set_layouts, {}, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);
}

void backend_upload_vert_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_VERTEX_BIT,
                            VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    backend->shader_indices.geometry_vert = 1;
}

void backend_upload_frag_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    backend->shader_indices.geometry_frag = 1;
}

void backend_upload_sky_box_shaders(VkBackend* backend, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                    const std::string& name) {

    /*
    Due to error in validation layers, creating linked shaders doesn't work during DEBUG.
    Due to this, I'll just create unlinked shaders for now, but I can improve this in the future
    by conditionally using linked shaders during release builds
   */
    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, vert_path, name, set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&backend->shader_ctx, frag_path, name, set_layouts, {}, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    backend->shader_indices.sky_box_vert = 2;
    backend->shader_indices.sky_box_frag = 2;
}

void backend_upload_cursor_shaders(VkBackend* backend) {
    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, "../shaders/vertex/cursor.vert", "cursor vert shader", set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT,
                            VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&backend->shader_ctx, "../shaders/fragment/cursor.frag", "cursor frag shader", set_layouts, {},
                            VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    backend->shader_indices.cursor_vert = 3;
    backend->shader_indices.cursor_frag = 3;
}

std::vector<const char*> get_instance_extensions() {
    uint32_t                 count{0};
    const char**             glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> extensions;
    for (size_t i = 0; i < count; i++) {
        extensions.emplace_back(glfw_extensions[i]);
    }
    if constexpr (vk_opts::validation_enabled) {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

void render_cursor(const VkBackend* backend, VkCommandBuffer cmd_buf) {

    vkCmdSetPrimitiveTopology(cmd_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vkCmdSetDepthTestEnable(cmd_buf, VK_FALSE);

    vkCmdSetCullMode(cmd_buf, VK_CULL_MODE_NONE);

    const uint16_t cursor_vert = backend->shader_indices.cursor_vert;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[cursor_vert].stage,
                                         &backend->shader_ctx.vert_shaders[cursor_vert].shader);

    const uint16_t cursor_frag = backend->shader_indices.cursor_frag;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[cursor_vert].stage,
                                         &backend->shader_ctx.frag_shaders[cursor_frag].shader);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->grid_pipeline_layout, 0, 1,
                            &backend->scene_desc_sets[backend->current_frame_i], 0, nullptr);

    vkCmdDraw(cmd_buf, 4, 1, 0, 0);
}

void backend_draw(VkBackend* backend, std::vector<Entity>& entities, const WorldData* scene_data, EngineFeatures engine_features) {
    auto start_frame_time = system_clock::now();

    update_curr_frame_idx(backend);
    Frame*          current_frame = &backend->frames[backend->current_frame_i];
    VkCommandBuffer cmd_buffer    = current_frame->command_context.primary_buffer;

    vkWaitForFences(backend->device_ctx.logical_device, 1, &current_frame->render_fence, VK_TRUE, vk_opts::timeout_dur);

    set_scene_data(backend, scene_data);

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(backend->device_ctx.logical_device, backend->swapchain_context.swapchain, vk_opts::timeout_dur,
                                            current_frame->present_semaphore, nullptr, &swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize(backend);
        return;
    }

    VK_CHECK(vkResetFences(backend->device_ctx.logical_device, 1, &current_frame->render_fence));

    VkImage swapchain_image = backend->swapchain_context.images[swapchain_image_index];

    command_ctx_begin_primary_buffer(&current_frame->command_context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // clear the entity id storage image
    vk_image_memory_barrier_insert(cmd_buffer, backend->entity_id_images[backend->current_frame_i].image, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkClearValue            entity_id_clear_value = {.color = {{0, 0, 0, 0}}};
    VkImageSubresourceRange range                 = vk_image_subresource_range_create(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0);

    vkCmdClearColorImage(cmd_buffer, backend->entity_id_images[backend->current_frame_i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &entity_id_clear_value.color, 1, &range);

    vk_image_memory_barrier_insert(cmd_buffer, backend->entity_id_images[backend->current_frame_i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL);

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, backend->depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd_buffer, &backend->scene_rendering_info);

    set_render_state(backend, cmd_buffer);

    if ((engine_features & EngineFeatures::SKY_BOX) == EngineFeatures::SKY_BOX) {
        render_sky_box(backend, cmd_buffer);
    }

    render_geometry(backend, cmd_buffer, entities);

    if ((engine_features & EngineFeatures::DEBUG_GRID) == EngineFeatures::DEBUG_GRID) {
        render_grid(backend, cmd_buffer);
    }

    render_cursor(backend, cmd_buffer);

    vkCmdEndRendering(cmd_buffer);

    if (backend->mode == EngineMode::EDIT) {
        // save image for UI viewport rendering into a dedicated image
        vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImage curr_viewport_img = backend->viewport_images[backend->current_frame_i].image;
        vk_image_memory_barrier_insert(cmd_buffer, curr_viewport_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vk_image_blit(cmd_buffer, backend->color_resolve_image.image, curr_viewport_img, backend->swapchain_context.extent, backend->image_extent);

        vk_image_memory_barrier_insert(cmd_buffer, curr_viewport_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdBeginRendering(cmd_buffer, &backend->ui_rendering_info);

        render_ui(cmd_buffer);

        vkCmdEndRendering(cmd_buffer);
    }

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_image_blit(cmd_buffer, backend->color_resolve_image.image, swapchain_image, backend->swapchain_context.extent, backend->image_extent);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkSemaphoreSubmitInfo wait_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSemaphoreSubmitInfo signal_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    command_ctx_submit_primary_buffer(&current_frame->command_context, backend->device_ctx.queues.graphics, current_frame->render_fence,
                                      &wait_semaphore_si, &signal_semaphore_si);

    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.pSwapchains        = &backend->swapchain_context.swapchain;
    present_info.swapchainCount     = 1;
    present_info.pImageIndices      = &swapchain_image_index;
    present_info.pWaitSemaphores    = &current_frame->render_semaphore;
    present_info.waitSemaphoreCount = 1;

    result = vkQueuePresentKHR(backend->device_ctx.queues.present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize(backend);
        return;
    }

    backend->frame_num++;

    auto end_time = system_clock::now();
    auto dur      = duration<float>(end_time - start_frame_time);
    backend->stats.total_fps += 1000000.f / static_cast<float>(duration_cast<microseconds>(dur).count());
    backend->stats.total_frame_time += duration_cast<microseconds>(dur).count();
    if (backend->frame_num % 60 == 0) {
        backend->stats.frame_time = duration_cast<microseconds>(dur).count() / 1000.f;
    }
}

void resize(VkBackend* backend) {
    vkDeviceWaitIdle(backend->device_ctx.logical_device);

    swapchain_ctx_reset(&backend->swapchain_context, &backend->device_ctx);

    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->depth_image);
    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_image);

    backend->image_extent.width  = backend->swapchain_context.extent.width;
    backend->image_extent.height = backend->swapchain_context.extent.height;

    backend->render_area.scissor_dimensions.x += backend->image_extent.width - backend->render_area.scissor_dimensions.x;
    backend->render_area.scissor_dimensions.y += backend->image_extent.height - backend->render_area.scissor_dimensions.y;

    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_resolve_image);
    backend->color_resolve_image =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);

    backend->color_image = allocated_image_create(
        backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, backend->device_ctx.raster_samples);

    backend->depth_image =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_D32_SFLOAT, 1, backend->device_ctx.raster_samples);

    if (backend->mode == EngineMode::EDIT) {
        for (size_t i = 0; i < backend->swapchain_context.images.size(); i++) {
            allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->viewport_images[i]);
            AllocatedImage new_viewport_image = allocated_image_create(
                backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);
            backend->viewport_images[i] = new_viewport_image;

            for (size_t j = 0; j < backend->viewport_images.size(); j++) {
                VkDescriptorSet new_viewport_ds = ImGui_ImplVulkan_AddTexture(backend->default_linear_sampler, backend->viewport_images[j].image_view,
                                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                backend->viewport_desc_sets[j]  = new_viewport_ds;
            }
        }
    }

    for (size_t i = 0; i < backend->entity_id_images.size(); i++) {
        allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->entity_id_images[i]);

        backend->entity_id_images[i] = allocated_image_create(
            backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16_UINT, 1, 1, VMA_MEMORY_USAGE_AUTO);

        DescriptorWriter writer{};
        desc_writer_write_image_desc(&writer, 1, backend->entity_id_images[i].image_view, backend->default_linear_sampler, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        desc_writer_update_desc_set(&writer, backend->device_ctx.logical_device, backend->scene_desc_sets[i]);
    }

    configure_render_resources(backend);

    for (Frame& frame : backend->frames) {
        frame_reset_synchronization(&frame, backend->device_ctx.logical_device);
    }
}

void render_geometry(VkBackend* backend, VkCommandBuffer cmd_buf, std::vector<Entity>& entities) {
    auto buffer_recording_start = system_clock::now();

    const VkDescriptorSet desc_sets[2] = {backend->scene_desc_sets[backend->current_frame_i], backend->graphics_desc_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->geometry_pipeline_layout, 0, 2, desc_sets, 0, nullptr);

    const auto record_obj = [&](const DrawObject* obj) {
        vkCmdPushConstants(cmd_buf, backend->geometry_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshData),
                           &obj->mesh_data);

        vkCmdBindIndexBuffer(cmd_buf, obj->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd_buf, obj->indices_count, 1, obj->indices_start, 0, 0);
    };

    const uint16_t geometry_vert = backend->shader_indices.geometry_vert;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[geometry_vert].stage,
                                         &backend->shader_ctx.vert_shaders[geometry_vert].shader);

    const uint16_t geometry_frag = backend->shader_indices.geometry_frag;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[geometry_frag].stage,
                                         &backend->shader_ctx.frag_shaders[geometry_frag].shader);

    backend->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    vkCmdSetDepthTestEnable(cmd_buf, VK_TRUE);

    vkCmdSetDepthWriteEnable(cmd_buf, VK_TRUE);

    for (const auto& entity : entities) {
        for (const DrawObject& obj : entity.opaque_objs) {
            record_obj(&obj);
        }
    }

    for (const auto& entity : entities) {
        VkColorBlendEquationEXT blend_equation = {};

        /*
                blend_equation = {
                    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
                    .colorBlendOp        = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp        = VK_BLEND_OP_ADD,
                };
        */

        blend_equation = {
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
        };
        backend->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &blend_equation);

        VkBool32 color_blend_enabled[] = {VK_TRUE};
        backend->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enabled);

        for (const DrawObject& obj : entity.transparent_objs) {
            record_obj(&obj);
        }
    }

    auto end_time = system_clock::now();
    auto dur      = duration<float>(end_time - buffer_recording_start);
    backend->stats.total_draw_time += static_cast<uint32_t>(duration_cast<microseconds>(dur).count());
    if (backend->frame_num % 60 == 0) {
        backend->stats.draw_time = duration_cast<microseconds>(dur).count();
    }
}

void render_ui(VkCommandBuffer cmd_buf) { ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf); }

void render_grid(const VkBackend* backend, VkCommandBuffer cmd_buf) {

    VkColorBlendEquationEXT blend_equation = {};

    blend_equation = {
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
    };
    backend->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &blend_equation);

    VkBool32 color_blend_enabled[] = {VK_TRUE};
    backend->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enabled);

    const uint16_t grid_vert = backend->shader_indices.grid_vert;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[grid_vert].stage,
                                         &backend->shader_ctx.vert_shaders[grid_vert].shader);

    const uint16_t grid_frag = backend->shader_indices.grid_frag;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[grid_frag].stage,
                                         &backend->shader_ctx.frag_shaders[grid_frag].shader);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->grid_pipeline_layout, 0, 1,
                            &backend->scene_desc_sets[backend->current_frame_i], 0, nullptr);

    vkCmdDraw(cmd_buf, 6, 1, 0, 0);
}

void set_render_state(VkBackend* backend, VkCommandBuffer cmd_buf) {

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = backend->image_extent.width;
    viewport.height   = backend->image_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D scissor_extent{};
    scissor_extent.width  = backend->render_area.scissor_dimensions.x;
    scissor_extent.height = backend->render_area.scissor_dimensions.y;

    VkOffset2D scissor_offset{};
    scissor_offset.x = backend->render_area.top_left.x;
    scissor_offset.y = backend->render_area.top_left.y;

    VkRect2D scissor{};
    scissor.extent = scissor_extent;
    scissor.offset = scissor_offset;

    vkCmdSetViewportWithCount(cmd_buf, 1, &viewport);

    vkCmdSetScissorWithCount(cmd_buf, 1, &scissor);

    vkCmdSetRasterizerDiscardEnable(cmd_buf, VK_FALSE);

    vkCmdSetCullMode(cmd_buf, VK_CULL_MODE_NONE);

    backend->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    VkColorBlendEquationEXT colorBlendEquationEXT{};
    backend->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &colorBlendEquationEXT);

    VkBool32 color_blend_enables[] = {VK_FALSE};
    backend->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enables);

    vkCmdSetPrimitiveTopology(cmd_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vkCmdSetPrimitiveRestartEnable(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdSetRasterizationSamplesEXT(cmd_buf, static_cast<VkSampleCountFlagBits>(backend->device_ctx.raster_samples));

    constexpr uint32_t max = ~0;

    constexpr VkSampleMask sample_masks[4] = {max, max, max, max};

    backend->ext_ctx.vkCmdSetSampleMaskEXT(cmd_buf, VK_SAMPLE_COUNT_4_BIT, sample_masks);

    backend->ext_ctx.vkCmdSetAlphaToCoverageEnableEXT(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdSetPolygonModeEXT(cmd_buf, VK_POLYGON_MODE_FILL);

    backend->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    backend->ext_ctx.vkCmdSetTessellationDomainOriginEXT(cmd_buf, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);

    backend->ext_ctx.vkCmdSetPatchControlPointsEXT(cmd_buf, 1);

    vkCmdSetLineWidth(cmd_buf, 1.0f);
    vkCmdSetFrontFace(cmd_buf, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthCompareOp(cmd_buf, VK_COMPARE_OP_GREATER_OR_EQUAL);
    vkCmdSetDepthTestEnable(cmd_buf, VK_FALSE);
    vkCmdSetDepthWriteEnable(cmd_buf, VK_TRUE);
    vkCmdSetDepthBoundsTestEnable(cmd_buf, VK_FALSE);
    vkCmdSetDepthBiasEnable(cmd_buf, VK_FALSE);
    vkCmdSetStencilTestEnable(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdSetLogicOpEnableEXT(cmd_buf, VK_FALSE);

    VkColorComponentFlags color_component_flags[] = {VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT};

    backend->ext_ctx.vkCmdSetColorWriteMaskEXT(cmd_buf, 0, 1, color_component_flags);

    // set default bindings (null) to all shader types for the graphics bind point
    // https://docs.vulkan.org/spec/latest/chapters/shaders.html#shaders-binding
    constexpr std::array graphics_pipeline_stages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_TASK_BIT_EXT,
        VK_SHADER_STAGE_MESH_BIT_EXT,
    };

    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, graphics_pipeline_stages.size(), graphics_pipeline_stages.data(), VK_NULL_HANDLE);
}

void upload_sky_box_texture(VkBackend* backend, const TextureSampler* tex_sampler) {

    assert(tex_sampler->layer_count == 6 && tex_sampler->view_type == VK_IMAGE_VIEW_TYPE_CUBE);

    const VkExtent2D extent{.width = tex_sampler->width, .height = tex_sampler->height};

    const uint32_t byte_size = tex_sampler->width * tex_sampler->height * tex_sampler->color_channels * tex_sampler->layer_count;

    AllocatedBuffer staging_buf = allocated_buffer_create(backend->allocator, byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, tex_sampler->mip_levels[0].data, staging_buf.allocation, 0, byte_size);

    const AllocatedImage new_texture =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               tex_sampler->view_type, extent, VK_FORMAT_R8G8B8A8_UNORM, 1);

    std::vector<VkBufferImageCopy> copy_regions;
    copy_regions.reserve(tex_sampler->layer_count);

    for (uint32_t face = 0; face < tex_sampler->layer_count; face++) {
        //  Calculate offset into staging buffer for the current mip level and face
        size_t offset = tex_sampler->width * tex_sampler->height * tex_sampler->color_channels * face;

        VkBufferImageCopy bufferCopyRegion               = {};
        bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel       = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = face;
        bufferCopyRegion.imageSubresource.layerCount     = 1;
        bufferCopyRegion.imageExtent.width               = tex_sampler->width;
        bufferCopyRegion.imageExtent.height              = tex_sampler->height;
        bufferCopyRegion.imageExtent.depth               = 1;
        bufferCopyRegion.bufferOffset                    = offset;
        copy_regions.push_back(bufferCopyRegion);
    }

    command_ctx_immediate_submit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics,
                                 backend->imm_fence, [&](VkCommandBuffer cmd) {
                                     vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 0, tex_sampler->layer_count);

                                     vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                            copy_regions.size(), copy_regions.data());

                                     vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, tex_sampler->layer_count);
                                 });

    DescriptorWriter descriptor_writer;
    desc_writer_write_image_desc(&descriptor_writer, backend->sky_box_desc_binding, new_texture.image_view, tex_sampler->sampler,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    desc_writer_update_desc_set(&descriptor_writer, backend->device_ctx.logical_device, backend->graphics_desc_set);

    allocated_buffer_destroy(backend->allocator, &staging_buf);
    backend->sky_box_image = new_texture;

    backend->deletion_queue.push_persistent(
        [=] { allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->sky_box_image); });
}

struct UniqueImageInstance {
    // const uint8_t* data;
    uint32_t color_channels{};
    uint32_t height{};
    uint32_t width{};
    // uint32_t       mip_levels{1};
    uint32_t              byte_size{};
    std::vector<MipLevel> mip_levels;
};

// [[nodiscard]] static std::vector<void*> compress_2d_color_textures(std::span<const UniqueImageInstance> image_instances,
//                                                                    uint32_t* total_compressed_size, uint32_t* largest_img_size) {
//
//     nvtt::Context context;
//     context.enableCudaAcceleration(true);
//
//     *largest_img_size = 0;
//     std::vector<void*> compressed_data_bufs;
//     compressed_data_bufs.reserve(image_instances.size());
//     uint32_t total_byte_size = 0;
//     for (size_t i = 0; i < image_instances.size(); i++) {
//         const auto& image = image_instances[i];
//         assert(image.color_channels == 4);
//
//         uint32_t image_bytes = image.height * image.width * image.color_channels;
//
//         nvtt::Surface nvtt_image;
//         bool          res = nvtt_image.loadFromMemory((void*)image.data, image_bytes);
//         assert(res);
//
//         nvtt::CompressionOptions compression_options;
//         compression_options.setFormat(nvtt::Format_BC7);
//
//         // void* compressed_data = malloc(compressed_data_bytes);
//
//         std::filesystem::path out_filename = "out_compressed_" + std::to_string(i);
//         std::ofstream         out_file(out_filename);
//
//         nvtt::OutputOptions output_options;
//         output_options.setFileName(out_filename.string().c_str());
//
//         std::ignore = context.compress(nvtt_image, 0, 0, compression_options, output_options);
//
//         const uint32_t compressed_data_bytes = std::filesystem::file_size(out_filename); // BC7 uses 16 bytes/tile
//         std::ifstream  file(out_filename, std::ios::binary | std::ios::ate);
//         assert(file);
//
//         void* out_buffer = malloc(compressed_data_bytes);
//
//         file.seekg(0, std::ios::beg);
//         file.read(static_cast<char*>(out_buffer), compressed_data_bytes);
//
//         *largest_img_size = (compressed_data_bytes > *largest_img_size) ? compressed_data_bytes : *largest_img_size;
//         compressed_data_bufs.push_back(out_buffer);
//         total_byte_size += compressed_data_bytes;
//
//         // nvtt::RefImage new_ref_image{};
//         // new_ref_image.data         = image.data;
//         // new_ref_image.num_channels = image.color_channels;
//         // new_ref_image.height       = image.height;
//         // new_ref_image.width        = image.width;
//         // new_ref_image.depth        = 1;
//         //
//         // nvtt::CPUInputBuffer input_buf(&new_ref_image, nvtt::UINT8);
//
//         //     const auto settings =
//         //     nvtt::EncodeSettings().SetFormat(nvtt::Format_BC7).SetQuality(nvtt::Quality_Normal).SetUseGPU(true).SetOutputToGPUMem(false);
//         // bool encoding_successful = nvtt_encode(input_buf, compressed_data, settings);
//         // assert(encoding_successful);
//     }
//
//     *total_compressed_size = total_byte_size;
//     return compressed_data_bufs;
// }

// CompressedImage compress_img_task(const UniqueImageInstance* image, uint32_t id) {
//
//     assert(image->color_channels == 4);
//     nvtt::RefImage new_ref_image{};
//     new_ref_image.data         = image->data;
//     new_ref_image.num_channels = image->color_channels;
//     new_ref_image.height       = image->height;
//     new_ref_image.width        = image->width;
//     new_ref_image.depth        = 1;
//
//     nvtt::CPUInputBuffer input_buf(&new_ref_image, nvtt::UINT8);
//
//     const uint32_t compressed_data_bytes = input_buf.NumTiles() * 16;
//
//     CompressedImage new_compressed_img{};
//     new_compressed_img.id   = id;
//     new_compressed_img.size = compressed_data_bytes;
//     new_compressed_img.data = malloc(compressed_data_bytes); // BC7 uses 16 bytes/tile
//
//     nvtt::TimingContext timing_ctx;
//
//     nvtt::EncodeSettings encode_settings;
//
//     const auto settings =
//         nvtt::EncodeSettings().SetFormat(nvtt::Format_BC7).SetQuality(nvtt::Quality_Fastest).SetUseGPU(false).SetOutputToGPUMem(false);
//     bool encoding_successful = nvtt_encode(input_buf, new_compressed_img.data, settings);
//     assert(encoding_successful);
//
//     //    std::cout << "finished id: " << id << std::endl;
//
//     return new_compressed_img;
// }

/*[[nodiscard]] static std::vector<CompressedImage> compress_2d_color_textures(std::span<const UniqueImageInstance> image_instances,
                                                                             uint32_t* total_compressed_size, uint32_t* largest_img_size) {

    // tf::Executor executor;
    *largest_img_size = 0;
    std::vector<std::future<CompressedImage>> compressed_data_buf_futures;
    compressed_data_buf_futures.reserve(image_instances.size());
    uint32_t total_byte_size = 0;
    // std::cout << "launching " << image_instances.size() << " async tasks" << std::endl;

    for (size_t i = 0; i < image_instances.size(); i++) {
        const auto& image = image_instances[i];
        compressed_data_buf_futures.push_back(std::async(std::launch::async, compress_img_task, &image, i));
    }

    std::vector<CompressedImage> compressed_images;
    compressed_images.reserve(compressed_data_buf_futures.size());
    for (size_t i = 0; i < compressed_data_buf_futures.size(); i++) {
        // std::cout << "getting " << i << std::endl;
        std::future<CompressedImage>& future = compressed_data_buf_futures[i];
        compressed_images.push_back(future.get());

        total_byte_size += compressed_images[i].size;

        *largest_img_size = (compressed_images[i].size > *largest_img_size) ? compressed_images[i].size : *largest_img_size;
    }

    std::ranges::sort(compressed_images, [&](const CompressedImage& e1, const CompressedImage& e2) { return e1.id < e2.id; });

    *total_compressed_size = total_byte_size;
    return compressed_images;
}*/

// [[nodiscard]] static std::vector<CompressedImage> compress_2d_color_textures(std::span<const UniqueImageInstance> image_instances,
//                                                                              uint32_t* total_compressed_size, uint32_t* largest_img_size) {
//
//     *largest_img_size = 0;
//     std::vector<CompressedImage> compressed_data_bufs;
//     compressed_data_bufs.reserve(image_instances.size());
//     uint32_t total_byte_size = 0;
//     for (size_t i = 0; i < image_instances.size(); i++) {
//         const UniqueImageInstance& image = image_instances[i];
//         assert(image.color_channels == 4);
//
//         nvtt::RefImage new_ref_image{};
//         new_ref_image.data         = image.data;
//         new_ref_image.num_channels = image.color_channels;
//         new_ref_image.height       = image.height;
//         new_ref_image.width        = image.width;
//         new_ref_image.depth        = 1;
//
//         nvtt::CPUInputBuffer input_buf(&new_ref_image, nvtt::UINT8);
//
//         const uint32_t compressed_data_bytes = input_buf.NumTiles() * 16;
//
//         *largest_img_size = (compressed_data_bytes > *largest_img_size) ? compressed_data_bytes : *largest_img_size;
//
//         CompressedImage new_compressed_img{};
//         new_compressed_img.id   = i;
//         new_compressed_img.size = compressed_data_bytes;
//         new_compressed_img.data = malloc(compressed_data_bytes); // BC7 uses 16 bytes/tile
//
//         const auto settings =
//             nvtt::EncodeSettings().SetFormat(nvtt::Format_BC7).SetQuality(nvtt::Quality_Fastest).SetUseGPU(true).SetOutputToGPUMem(false);
//         bool encoding_successful = nvtt_encode(input_buf, new_compressed_img.data, settings);
//         assert(encoding_successful);
//
//         compressed_data_bufs.push_back(new_compressed_img);
//         total_byte_size += compressed_data_bytes;
//     }
//
//     *total_compressed_size = total_byte_size;
//     return compressed_data_bufs;
// }

void generate_mip_maps(VkBackend* backend, AllocatedImage* allocated_img, VkCommandBuffer cmd_buf) {
    uint32_t mip_width  = allocated_img->image_extent.width;
    uint32_t mip_height = allocated_img->image_extent.height;

    for (size_t i = 1; i < allocated_img->mip_levels; i++) {
        vk_image_memory_barrier_insert(cmd_buf, allocated_img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1,
                                       i - 1);

        VkExtent2D src_extent = {.width = mip_width, .height = mip_height};
        VkExtent2D dst_extent = {.width = mip_width > 1 ? mip_width / 2 : 1, .height = mip_height > 1 ? mip_height / 2 : 1};

        vk_image_blit(cmd_buf, allocated_img->image, allocated_img->image, src_extent, dst_extent, i - 1, i);

        vk_image_memory_barrier_insert(cmd_buf, allocated_img->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       1, i - 1);

        // if we're at the last image, we never blit from it. so transition from dst optimal instead of src optimal
        if (i == allocated_img->mip_levels - 1) {
            vk_image_memory_barrier_insert(cmd_buf, allocated_img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, i);
        }

        mip_width  = mip_width > 1 ? mip_width / 2 : 1;
        mip_height = mip_height > 1 ? mip_height / 2 : 1;
    }
}

uint32_t backend_upload_2d_textures(VkBackend* backend, std::vector<TextureSampler>& tex_samplers, VkFormat format) {
    // we will return this at the end of the function. It signifies an offset for
    // materials accessing these textures by their index.
    // For instance, if a gltf mesh is trying to access texture index 3, and this function passes
    // back the number 5, then the mesh should point to texture index 8 since this is where the
    // descriptor for this texture will actually be found in the shader
    const uint32_t descriptor_index_offset = backend->tex_sampler_desc_count;

    uint32_t largest_image_size = 0;

    std::vector<UniqueImageInstance> unique_image_instances;
    unique_image_instances.reserve(tex_samplers.size());
    for (const auto& tex_sampler : tex_samplers) {
        assert(tex_sampler.mip_levels.size() >= 1);
        assert(tex_sampler.mip_levels[0].height == tex_sampler.height);
        assert(tex_sampler.mip_levels[0].width == tex_sampler.width);

        UniqueImageInstance new_image{};
        // new_image.data           = tex_sampler.data;
        new_image.height         = tex_sampler.height;
        new_image.width          = tex_sampler.width;
        new_image.color_channels = tex_sampler.color_channels;
        // new_image.mip_levels     = tex_sampler.mip_count;
        // new_image.byte_size      = tex_sampler.byte_size;
        new_image.mip_levels = tex_sampler.mip_levels;

        bool is_unique = true;
        for (const auto& curr_image : unique_image_instances) {
            // isn't unique if the data and dimensions differ
            if (new_image.mip_levels[0].data == curr_image.mip_levels[0].data && new_image.height == curr_image.height &&
                new_image.width == curr_image.width) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            if (largest_image_size < tex_sampler.height * tex_sampler.width * tex_sampler.color_channels) {
                largest_image_size = tex_sampler.height * tex_sampler.width * tex_sampler.color_channels;
            }
            unique_image_instances.push_back(new_image);
        }
    }

    uint32_t total_byte_size = 0;
    // uint32_t largest_image_size = 0;

    // std::vector<CompressedImage> compressed_data_bufs = compress_2d_color_textures(unique_image_instances, &total_byte_size, &largest_image_size);
    //   use the largest image size for our staging buffer, then just reuse it for all images
    AllocatedBuffer staging_buf = allocated_buffer_create(backend->allocator, largest_image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    DescriptorWriter descriptor_writer{};
    for (size_t i = 0; i < unique_image_instances.size(); i++) {
        const UniqueImageInstance* image = &unique_image_instances[i];

        VkExtent2D extent = {
            .width  = image->width,
            .height = image->height,
        };

        AllocatedImage tex_image =
            allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_IMAGE_VIEW_TYPE_2D, extent, format, image->mip_levels.size());

        command_ctx_immediate_submit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics,
                                     backend->imm_fence, [&](VkCommandBuffer cmd) {
                                         // transition all mips to transfer destination
                                         vk_image_memory_barrier_insert(cmd, tex_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex_image.mip_levels);
                                         for (size_t mip = 0; mip < image->mip_levels.size(); mip++) {
                                             const MipLevel* mip_level = &image->mip_levels[mip];
                                             // copy this mip to staging
                                             vmaCopyMemoryToAllocation(backend->allocator, image->mip_levels[mip].data, staging_buf.allocation, 0,
                                                                       mip_level->height * mip_level->width);

                                             VkBufferImageCopy copy_region{};
                                             copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                                             copy_region.imageSubresource.mipLevel       = mip;
                                             copy_region.imageSubresource.baseArrayLayer = 0;
                                             copy_region.imageSubresource.layerCount     = 1;
                                             copy_region.imageExtent.width               = mip_level->width;
                                             copy_region.imageExtent.height              = mip_level->height;
                                             copy_region.imageExtent.depth               = 1;
                                             copy_region.bufferOffset                    = 0;

                                             vkCmdCopyBufferToImage(cmd, staging_buf.buffer, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                                                    &copy_region);
                                         }
                                         // transition all mips back to read only
                                         vk_image_memory_barrier_insert(cmd, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tex_image.mip_levels);
                                     });

        backend->tex_images.push_back(tex_image);
    }

    // this is going to be a bit ugly :/...
    for (const auto& tex_sampler : tex_samplers) {
        // find which index of unique images this tex_sampler refers to
        uint32_t image_i = 0;
        for (size_t i = 0; i < unique_image_instances.size(); i++) {
            if (tex_sampler.mip_levels[0].data == unique_image_instances[i].mip_levels[0].data) {
                image_i = i;
                break;
            }
        }

        // find index into our GPU allocated texture images based on this
        uint32_t tex_img_i = backend->tex_images.size() - unique_image_instances.size() + image_i;

        const AllocatedImage* tex_image = &backend->tex_images[tex_img_i];

        backend->mip_mapped_samplers.push_back(
            vk_sampler_create(backend->device_ctx.logical_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, 0.f, tex_image->mip_levels));

        desc_writer_write_image_desc(&descriptor_writer, backend->texture_desc_binding, tex_image->image_view,
                                     backend->mip_mapped_samplers[backend->mip_mapped_samplers.size() - 1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, backend->tex_sampler_desc_count++);
        desc_writer_update_desc_set(&descriptor_writer, backend->device_ctx.logical_device, backend->graphics_desc_set);
        desc_writer_clear(&descriptor_writer);
    }

    // for (auto& data : compressed_data_bufs) {
    //     // free(data);
    //     free(data.data);
    // }
    allocated_buffer_destroy(backend->allocator, &staging_buf);

    return descriptor_index_offset;
}

void render_sky_box(const VkBackend* backend, VkCommandBuffer cmd_buf) {

    vkCmdSetDepthWriteEnable(cmd_buf, VK_FALSE);

    const uint16_t sky_box_vert = backend->shader_indices.sky_box_vert;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[sky_box_vert].stage,
                                         &backend->shader_ctx.vert_shaders[sky_box_vert].shader);

    const uint16_t sky_box_frag = backend->shader_indices.sky_box_frag;
    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[sky_box_frag].stage,
                                         &backend->shader_ctx.frag_shaders[sky_box_frag].shader);

    VkVertexInputBindingDescription2EXT input_description{};
    input_description.sType     = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
    input_description.binding   = 0;
    input_description.stride    = sizeof(float) * 3;
    input_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    input_description.divisor   = 1;

    VkVertexInputAttributeDescription2EXT attribute_description{};
    attribute_description.sType   = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
    attribute_description.binding = 0;
    attribute_description.format  = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_description.offset  = 0;

    backend->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 1, &input_description, 1, &attribute_description);

    const VkDescriptorSet desc_sets[2] = {backend->scene_desc_sets[backend->current_frame_i], backend->graphics_desc_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->sky_box_pipeline_layout, 0, 2, desc_sets, 0, nullptr);

    VkDeviceSize offsets = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &backend->sky_box_vert_buffer.buffer, &offsets);

    vkCmdDraw(cmd_buf, 36, 1, 0, 0);
}

VkShaderModule load_shader_module(const VkBackend* backend, std::span<uint32_t> shader_spv) {

    VkShaderModule           shader_module;
    VkShaderModuleCreateInfo shader_module_ci{};
    shader_module_ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_ci.codeSize = shader_spv.size() * sizeof(uint32_t);
    shader_module_ci.pCode    = shader_spv.data();

    VK_CHECK(vkCreateShaderModule(backend->device_ctx.logical_device, &shader_module_ci, nullptr, &shader_module));

    return shader_module;
}

void backend_create_accel_struct(VkBackend* backend, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                 std::span<const TopLevelInstanceRef> instance_refs) {
    accel_struct_ctx_add_triangles_geometry(&backend->accel_struct_ctx, backend->device_ctx.logical_device, backend->allocator, &backend->ext_ctx,
                                            backend->device_ctx.queues.graphics, bottom_level_geometries, instance_refs);

    DescriptorWriter desc_writer;
    desc_writer_write_accel_struct_desc(&desc_writer, backend->accel_struct_desc_binding, &backend->accel_struct_ctx.top_level,
                                        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    desc_writer_update_desc_set(&desc_writer, backend->device_ctx.logical_device, backend->graphics_desc_set);
};

void backend_update_accel_struct(VkBackend* backend, const glm::mat4* transform, uint32_t instance_idx) {
    accel_struct_ctx_update_tlas(&backend->accel_struct_ctx, &backend->ext_ctx, backend->device_ctx.logical_device,
                                 backend->device_ctx.queues.graphics, backend->allocator, transform, instance_idx);
}

void backend_finish_pending_vk_work(const VkBackend* backend) { vkDeviceWaitIdle(backend->device_ctx.logical_device); }

void backend_deinit(VkBackend* backend) {
    DEBUG_PRINT("destroying Vulkan Backend");

    fmt::println("average backend_draw time: {:.3f} us", static_cast<float>(backend->stats.total_draw_time) / static_cast<float>(backend->frame_num));
    fmt::println("average frame time: {:.3f} ms",
                 static_cast<float>(backend->stats.total_frame_time) / 1000.f / static_cast<float>(backend->frame_num));
    fmt::println("average fps: {:.3f}", static_cast<float>(backend->stats.total_fps) / static_cast<float>(backend->frame_num));

    backend->deletion_queue.flush();

    if constexpr (vk_opts::validation_enabled) {
        debugger_deinit(&backend->debugger, backend->instance);
    }

    for (Frame& frame : backend->frames) {
        frame_deinit(&frame, backend->device_ctx.logical_device);
    }

    command_ctx_deinit(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device);
    command_ctx_deinit(&backend->compute_cmd_ctx, backend->device_ctx.logical_device);

    shader_ctx_deinit(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device);

    accel_struct_ctx_deinit(&backend->accel_struct_ctx, &backend->ext_ctx, backend->allocator, backend->device_ctx.logical_device);

    swapchain_ctx_deinit(&backend->swapchain_context, backend->device_ctx.logical_device, backend->instance);

    desc_allocator_deinit(&backend->scene_desc_allocator, backend->device_ctx.logical_device);
    desc_allocator_deinit(&backend->graphics_desc_allocator, backend->device_ctx.logical_device);

    for (const auto& tex_image : backend->tex_images) {
        allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &tex_image);
    }

    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_image);
    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->depth_image);
    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_resolve_image);

    if (backend->mode == EngineMode::EDIT) {
        for (const auto& image : backend->viewport_images) {
            allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &image);
        }
    }

    for (const auto& entity_id_img : backend->entity_id_images) {
        allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &entity_id_img);
    }

    for (const auto& scene_buf : backend->scene_data_buffers) {
        allocated_buffer_destroy(backend->allocator, &scene_buf);
    }

    allocated_buffer_destroy(backend->allocator, &backend->mat_buffer);
    allocated_buffer_destroy(backend->allocator, &backend->entity_id_result_buffer);

    vkDestroySampler(backend->device_ctx.logical_device, backend->default_nearest_sampler, nullptr);
    vkDestroySampler(backend->device_ctx.logical_device, backend->default_linear_sampler, nullptr);

    vkDestroyPipelineLayout(backend->device_ctx.logical_device, backend->grid_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(backend->device_ctx.logical_device, backend->geometry_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(backend->device_ctx.logical_device, backend->sky_box_pipeline_layout, nullptr);

    vkDestroyDescriptorPool(backend->device_ctx.logical_device, backend->imm_descriptor_pool, nullptr);

    vkDestroyDescriptorSetLayout(backend->device_ctx.logical_device, backend->scene_desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(backend->device_ctx.logical_device, backend->graphics_desc_set_layout, nullptr);
    vkDestroyFence(backend->device_ctx.logical_device, backend->imm_fence, nullptr);

    vmaDestroyAllocator(backend->allocator);

    device_ctx_deinit(&backend->device_ctx);

    vkDestroyInstance(backend->instance, nullptr);
}