
#include <array>
#include <iostream>
#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "global_utils.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "system/device/vk_command.h"
#include "system/device/vk_options.h"
#include "system/device/vk_sync.h"
#include "vk_init.h"
#include "vk_pipeline.h"
#include "vk_renderer.h"
#include "vk_swapchain.h"
#include <chrono>
#include <cstring>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fstream>
#include <future>
#include <system/device/vk_utils.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <core/engine.h>

// initialization
static void create_allocator(Renderer* renderer);
static void create_default_data(Renderer* renderer);
static void create_desc_layouts(Renderer* renderer);
static void upload_sky_box_texture(Renderer* renderer, const TextureSampler* tex_sampler);
static void create_graphics_desc_set(Renderer* renderer);
static void create_scene_desc_sets(Renderer* renderer);
static void create_grid_pipeline(Renderer* renderer);
static void create_pipeline_layouts(Renderer* renderer);
static void create_sky_box(Renderer* renderer);
static void create_compute_resources(Renderer* renderer);
static void create_render_images(Renderer* renderer);
static void configure_debugger(const Renderer* renderer);
static void configure_render_resources(Renderer* renderer);

static VkShaderModule load_shader_module(const Renderer* renderer, std::span<uint32_t> shader_spv);
// state update
static void resize(Renderer* renderer);
static void set_render_state(Renderer* renderer, VkCommandBuffer cmd_buf);
static void set_scene_data(const Renderer* renderer, const WorldData* scene_data);
// rendering
void        render_geometry(Renderer* renderer, VkCommandBuffer cmd_buf, std::vector<Entity>& entities);
static void render_ui(VkCommandBuffer cmd_buf);
static void render_grid(const Renderer* renderer, VkCommandBuffer cmd_buf);
static void render_sky_box(const Renderer* renderer, VkCommandBuffer cmd_buf);
static void renderer_upload_grid_shaders(Renderer* renderer, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                         const std::string& name);

static void update_curr_frame_idx(Renderer* renderer) { renderer->current_frame_i = renderer->frame_num % renderer->frames.size(); }

using namespace std::chrono;

static Renderer* active_renderer = nullptr;

void renderer_init(Renderer* renderer, const VkContext* vk_ctx, uint32_t width, uint32_t height, EngineMode mode) {

    assert(active_renderer == nullptr);
    active_renderer = renderer;

    renderer->vk_ctx = vk_ctx;

    swapchain_ctx_init(&renderer->swapchain_context, vk_ctx, vk_opts::desired_present_mode);

    create_allocator(renderer);
    create_desc_layouts(renderer);
    create_graphics_desc_set(renderer);
    create_scene_desc_sets(renderer);

    create_sky_box(renderer);

    renderer->image_extent.width  = width;
    renderer->image_extent.height = height;

    // attempt to do 2x2 MSAA
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vk_ctx->physical_device, &properties);
    renderer->msaa_samples = std::min(properties.limits.framebufferColorSampleCounts, 4u);

    for (size_t i = 0; i < renderer->frames.size(); i++) {
        Frame* frame = &renderer->frames[i];
        frame_init(frame, vk_ctx->logical_device, renderer->allocator, vk_ctx->queue_families.graphics);

        renderer->scene_data_buffers[i] =
            allocated_buffer_create(renderer->allocator, sizeof(WorldData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    renderer->entity_id_result_buffer =
        allocated_buffer_create(renderer->allocator, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

    RenderArea initial_render_area{};
    initial_render_area.scissor_dimensions = glm::vec2{width, height};

    renderer->render_area = initial_render_area;

    renderer->mode = mode;

    renderer->imm_fence = vk_fence_create(vk_ctx->logical_device, VK_FENCE_CREATE_SIGNALED_BIT);

    command_ctx_init(&renderer->immediate_cmd_ctx, vk_ctx->logical_device, vk_ctx->queue_families.graphics,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    command_ctx_init(&renderer->compute_cmd_ctx, vk_ctx->logical_device, vk_ctx->queue_families.compute,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    accel_struct_ctx_init(&renderer->accel_struct_ctx, vk_ctx->logical_device, vk_ctx->queue_families.graphics);

    create_pipeline_layouts(renderer);

    create_compute_resources(renderer);

    create_default_data(renderer);

    create_render_images(renderer);

    configure_render_resources(renderer);

    shader_ctx_init(&renderer->shader_ctx);

    ext_context_init(&renderer->ext_ctx, vk_ctx->logical_device);

    create_grid_pipeline(renderer);

    if constexpr (vk_opts::validation_enabled) {
        configure_debugger(renderer);
    }
}

static void create_compute_resources(Renderer* renderer) {
    // TODO: create resources
}

uint16_t renderer_entity_id_at_pos(const Renderer* renderer, int32_t x, int32_t y) {

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

    command_ctx_immediate_submit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                 renderer->imm_fence, [&](VkCommandBuffer cmd) {
                                     vkCmdCopyImageToBuffer(cmd, renderer->entity_id_images[renderer->current_frame_i].image, VK_IMAGE_LAYOUT_GENERAL,
                                                            renderer->entity_id_result_buffer.buffer, 1, &region);
                                 });

    return (*static_cast<uint32_t*>(renderer->entity_id_result_buffer.info.pMappedData));
}

void set_scene_data(const Renderer* renderer, const WorldData* scene_data) {
    assert(scene_data);
    assert(renderer->current_frame_i < renderer->scene_data_buffers.size());
    VkDescriptorSet        curr_scene_desc_set = renderer->scene_desc_sets[renderer->current_frame_i];
    const AllocatedBuffer* curr_scene_buf      = &renderer->scene_data_buffers[renderer->current_frame_i];

    VK_CHECK(vmaCopyMemoryToAllocation(renderer->allocator, scene_data, curr_scene_buf->allocation, 0, sizeof(WorldData)));

    DescriptorWriter desc_writer{};
    desc_writer_write_buffer_desc(&desc_writer, 0, curr_scene_buf->buffer, sizeof(WorldData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    desc_writer_update_desc_set(&desc_writer, renderer->vk_ctx->logical_device, curr_scene_desc_set);
}

static constexpr std::array skybox_vertices = {
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
};

void create_sky_box(Renderer* renderer) {
    renderer->sky_box_vert_buffer =
        allocated_buffer_create(renderer->allocator, 36 * sizeof(glm::vec3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(renderer->allocator, skybox_vertices.data(), renderer->sky_box_vert_buffer.allocation, 0,
                              renderer->sky_box_vert_buffer.info.size);

    renderer->deletion_queue.push_persistent([=] { allocated_buffer_destroy(renderer->allocator, &renderer->sky_box_vert_buffer); });
};

void renderer_upload_sky_box(Renderer* renderer, const uint8_t* texture_data, uint32_t color_channels, uint32_t width, uint32_t height) {
    TextureSampler sky_box_tex_sampler{};
    sky_box_tex_sampler.sampler   = renderer->default_linear_sampler;
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

    upload_sky_box_texture(renderer, &sky_box_tex_sampler);
}

void create_pipeline_layouts(Renderer* renderer) {

    std::array set_layouts = {renderer->scene_desc_set_layout, renderer->graphics_desc_set_layout};

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    renderer->geometry_pipeline_layout = vk_pipeline_layout_create(renderer->vk_ctx->logical_device, set_layouts, push_constant_ranges, 0);

    renderer->sky_box_pipeline_layout = vk_pipeline_layout_create(renderer->vk_ctx->logical_device, set_layouts, {}, 0);
}

static void create_grid_pipeline(Renderer* renderer) {

    renderer_upload_grid_shaders(renderer, "../shaders/vertex/grid.vert", "../shaders/fragment/grid.frag", "grid shaders");

    std::array set_layouts{renderer->scene_desc_set_layout};

    // TODO: dont hardcode these
    renderer->shader_indices.grid_vert = 0;
    renderer->shader_indices.grid_frag = 0;

    renderer->grid_pipeline_layout = vk_pipeline_layout_create(renderer->vk_ctx->logical_device, set_layouts, {}, 0);
}

// adding names to these 64 bit handles helps a lot when reading validation errors
void configure_debugger(const Renderer* renderer) {
    const Debugger* debugger = &renderer->vk_ctx->debugger;

    // debugger_init(&renderer->debugger, renderer->instance, renderer->vk_ctx->logical_device);
    debugger_set_handle_name(debugger, renderer->color_msaa_image.image, VK_OBJECT_TYPE_IMAGE, "color msaa image");
    debugger_set_handle_name(debugger, renderer->color_msaa_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color msaa image view");
    debugger_set_handle_name(debugger, renderer->depth_image.image, VK_OBJECT_TYPE_IMAGE, "depth image");
    debugger_set_handle_name(debugger, renderer->depth_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "depth image view");
    debugger_set_handle_name(debugger, renderer->color_image.image, VK_OBJECT_TYPE_IMAGE, "color image");
    debugger_set_handle_name(debugger, renderer->color_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color image view");
    debugger_set_handle_name(debugger, renderer->immediate_cmd_ctx.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "imm cmd_buf buf");

    for (size_t i = 0; i < renderer->frames.size(); i++) {
        const Frame& frame = renderer->frames[i];
        debugger_set_handle_name(debugger, frame.command_context.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                 "frame " + std::to_string(i) + " cmd_buf buf");
        debugger_set_handle_name(debugger, renderer->entity_id_images[i].image, VK_OBJECT_TYPE_IMAGE, "entity id image " + std::to_string(i));
        debugger_set_handle_name(debugger, renderer->entity_id_images[i].image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                                 "entity id image view " + std::to_string(i));
    }

    for (size_t i = 0; i < renderer->swapchain_context.images.size(); i++) {
        debugger_set_handle_name(debugger, renderer->swapchain_context.images[i], VK_OBJECT_TYPE_IMAGE, "swapchain image " + std::to_string(i));
        debugger_set_handle_name(debugger, renderer->swapchain_context.image_views[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                                 "swapchain image view " + std::to_string(i));
    }
}

void create_desc_layouts(Renderer* renderer) {
    DescriptorLayoutBuilder layout_builder;
    // scene data
    desc_layout_builder_add_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // entity id data
    desc_layout_builder_add_binding(&layout_builder, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    renderer->scene_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, renderer->vk_ctx->logical_device);

    desc_layout_builder_clear(&layout_builder);

    // materials
    desc_layout_builder_add_binding(&layout_builder, renderer->material_desc_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // acceleration structure
    desc_layout_builder_add_binding(&layout_builder, renderer->accel_struct_desc_binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // sky box
    desc_layout_builder_add_binding(&layout_builder, renderer->sky_box_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // textures
    constexpr VkDescriptorBindingFlags texture_binding_flags =
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    desc_layout_builder_add_binding(&layout_builder, renderer->texture_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, texture_binding_flags, vk_opts::texture_desc_limit);

    renderer->graphics_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, renderer->vk_ctx->logical_device);
}

// creates a scene desc set per frame
void create_scene_desc_sets(Renderer* renderer) {
    std::array<PoolSizeRatio, 2> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3}},
    };
    desc_allocator_init(&renderer->scene_desc_allocator, renderer->vk_ctx->logical_device, 3, pool_sizes);

    for (size_t i = 0; i < renderer->scene_desc_sets.size(); i++) {
        renderer->scene_desc_sets[i] =
            desc_allocator_allocate_desc_set(&renderer->scene_desc_allocator, renderer->vk_ctx->logical_device, renderer->scene_desc_set_layout);
    }
}

void create_graphics_desc_set(Renderer* renderer) {
    std::array<PoolSizeRatio, 4> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
         {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk_opts::texture_desc_limit}}
    };
    desc_allocator_init(&renderer->graphics_desc_allocator, renderer->vk_ctx->logical_device, 1, pool_sizes);

    renderer->graphics_desc_set = desc_allocator_allocate_desc_set(&renderer->graphics_desc_allocator, renderer->vk_ctx->logical_device,
                                                                   renderer->graphics_desc_set_layout, vk_opts::texture_desc_limit);
}

void configure_render_resources(Renderer* renderer) {

    VkClearValue scene_clear_value = {.color = {{0.f, 0.f, 0.f, 0.f}}};

    VkImageView draw_image_view    = renderer->color_image.image_view;
    VkImageView resolve_image_view = nullptr;
    if (renderer->msaa_samples > 1) {
        draw_image_view    = renderer->color_msaa_image.image_view;
        resolve_image_view = renderer->color_image.image_view;
    }

    renderer->scene_color_attachment = vk_color_attachment_info_create(draw_image_view, &scene_clear_value, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                                       VK_ATTACHMENT_STORE_OP_STORE, resolve_image_view);

    renderer->scene_depth_attachment =
        vk_depth_attachment_info_create(renderer->depth_image.image_view, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

    std::array scene_color_attachments = {&renderer->scene_color_attachment};
    renderer->scene_rendering_info     = vk_rendering_info_create(scene_color_attachments, &renderer->scene_depth_attachment, renderer->image_extent);

    renderer->ui_color_attachment =
        vk_color_attachment_info_create(renderer->color_image.image_view, nullptr, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

    std::array ui_color_attachments = {&renderer->ui_color_attachment};
    renderer->ui_rendering_info     = vk_rendering_info_create(ui_color_attachments, nullptr, renderer->image_extent);
}

void create_default_data(Renderer* renderer) {

    renderer->stats.total_fps        = 0;
    renderer->stats.total_frame_time = 0;
    renderer->stats.total_draw_time  = 0;

    renderer->default_linear_sampler = vk_sampler_create(renderer->vk_ctx->logical_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR);

    renderer->default_nearest_sampler = vk_sampler_create(renderer->vk_ctx->logical_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

    uint32_t white_data[64];
    for (size_t i = 0; i < 64; i++) {
        white_data[i] = 0xFFFFFFFF;
    }
    TextureSampler default_tex_sampler{};
    default_tex_sampler.width          = 4;
    default_tex_sampler.height         = 4;
    default_tex_sampler.color_channels = 4;
    default_tex_sampler.layer_count    = 1;
    default_tex_sampler.sampler        = renderer->default_linear_sampler;
    default_tex_sampler.view_type      = VK_IMAGE_VIEW_TYPE_2D;
    default_tex_sampler.format         = VK_FORMAT_R8G8B8A8_UNORM;

    MipLevel new_mip_level{};
    new_mip_level.data   = &white_data;
    new_mip_level.height = 4;
    new_mip_level.width  = 4;

    default_tex_sampler.mip_levels.push_back(new_mip_level);

    // default_tex_sampler.data      = reinterpret_cast<const uint8_t*>(&white_data);
    // default_tex_sampler.byte_size = 4 * 4 * 4;

    std::vector<TextureSampler> tex_samplers = {default_tex_sampler};

    // default texture will always be assumed to be at index 0
    std::ignore = renderer_upload_2d_textures(renderer, tex_samplers, 4);
}

void renderer_create_imgui_resources(Renderer* renderer) {

    assert(renderer->mode == EngineMode::EDIT);

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

    VK_CHECK(vkCreateDescriptorPool(renderer->vk_ctx->logical_device, &pool_ci, nullptr, &renderer->imm_descriptor_pool));

    VkPipelineRenderingCreateInfoKHR pipeline_info{};
    pipeline_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_info.pColorAttachmentFormats = &renderer->color_image.image_format;
    pipeline_info.colorAttachmentCount    = 1;
    pipeline_info.depthAttachmentFormat   = renderer->depth_image.image_format;

    ImGui_ImplVulkan_InitInfo init_info   = {};
    init_info.Instance                    = renderer->vk_ctx->instance;
    init_info.PhysicalDevice              = renderer->vk_ctx->physical_device;
    init_info.Device                      = renderer->vk_ctx->logical_device;
    init_info.Queue                       = renderer->vk_ctx->queues.graphics;
    init_info.DescriptorPool              = renderer->imm_descriptor_pool;
    init_info.MinImageCount               = 3;
    init_info.ImageCount                  = 3;
    init_info.UseDynamicRendering         = true;
    init_info.PipelineRenderingCreateInfo = pipeline_info;
    init_info.MSAASamples                 = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    for (size_t i = 0; i < renderer->viewport_images.size(); i++) {
        VkDescriptorSet new_viewport_ds = ImGui_ImplVulkan_AddTexture(renderer->default_linear_sampler, renderer->viewport_images[i].image_view,
                                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        renderer->viewport_desc_sets.push_back(new_viewport_ds);
    }
}
void renderer_update_render_area(Renderer* renderer, const RenderArea* render_area) { renderer->render_area = *render_area; }

void create_allocator(Renderer* renderer) {
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.device         = renderer->vk_ctx->logical_device;
    allocator_info.physicalDevice = renderer->vk_ctx->physical_device;
    allocator_info.instance       = renderer->vk_ctx->instance;
    allocator_info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocator_info, &renderer->allocator));
}

void renderer_recompile_frag_shader(Renderer* renderer, uint32_t shader_idx) {
    assert(shader_idx < renderer->shader_ctx.frag_shaders.size());
    VK_CHECK(vkDeviceWaitIdle(renderer->vk_ctx->logical_device));

    DEBUG_PRINT("recompiling shaders\n");
    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        renderer->scene_desc_set_layout,
        renderer->graphics_desc_set_layout,
    };
    Shader* shader = &renderer->shader_ctx.frag_shaders[shader_idx];

    shader_ctx_stage_shader(&renderer->shader_ctx, shader->path, shader->name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_replace_shader(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked, shader_idx);
}

void renderer_upload_grid_shaders(Renderer* renderer, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                  const std::string& name) {

    std::array set_layouts{
        renderer->scene_desc_set_layout,
    };

    shader_ctx_stage_shader(&renderer->shader_ctx, vert_path, name, set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&renderer->shader_ctx, frag_path, name, set_layouts, {}, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);
}

void renderer_upload_vert_shader(Renderer* renderer, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        renderer->scene_desc_set_layout,
        renderer->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&renderer->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_VERTEX_BIT,
                            VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    renderer->shader_indices.geometry_vert = 1;
}

void renderer_upload_frag_shader(Renderer* renderer, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array push_constant_ranges = {
        VkPushConstantRange{
                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            .offset     = 0,
                            .size       = sizeof(MeshData),
                            }
    };

    std::array set_layouts{
        renderer->scene_desc_set_layout,
        renderer->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&renderer->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    renderer->shader_indices.geometry_frag = 1;
}

void renderer_upload_sky_box_shaders(Renderer* renderer, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path,
                                     const std::string& name) {

    /*
    Due to error in validation layers, creating linked shaders doesn't work during DEBUG.
    Due to this, I'll just create unlinked shaders for now, but I can improve this in the future
    by conditionally using linked shaders during release builds
   */
    std::array set_layouts{
        renderer->scene_desc_set_layout,
        renderer->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&renderer->shader_ctx, vert_path, name, set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&renderer->shader_ctx, frag_path, name, set_layouts, {}, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    renderer->shader_indices.sky_box_vert = 2;
    renderer->shader_indices.sky_box_frag = 2;
}

void renderer_upload_cursor_shaders(Renderer* renderer) {
    std::array set_layouts{
        renderer->scene_desc_set_layout,
        renderer->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&renderer->shader_ctx, "../shaders/vertex/cursor.vert", "cursor vert shader", set_layouts, {}, VK_SHADER_STAGE_VERTEX_BIT,
                            VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    shader_ctx_stage_shader(&renderer->shader_ctx, "../shaders/fragment/cursor.frag", "cursor frag shader", set_layouts, {},
                            VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, ShaderType::unlinked);

    // TODO: dont hardcode these
    renderer->shader_indices.cursor_vert = 3;
    renderer->shader_indices.cursor_frag = 3;
}

void render_cursor(const Renderer* renderer, VkCommandBuffer cmd_buf) {

    vkCmdSetPrimitiveTopology(cmd_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

    vkCmdSetDepthTestEnable(cmd_buf, VK_FALSE);

    vkCmdSetCullMode(cmd_buf, VK_CULL_MODE_NONE);

    const uint16_t cursor_vert = renderer->shader_indices.cursor_vert;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.vert_shaders[cursor_vert].stage,
                                          &renderer->shader_ctx.vert_shaders[cursor_vert].shader);

    const uint16_t cursor_frag = renderer->shader_indices.cursor_frag;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.frag_shaders[cursor_vert].stage,
                                          &renderer->shader_ctx.frag_shaders[cursor_frag].shader);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->grid_pipeline_layout, 0, 1,
                            &renderer->scene_desc_sets[renderer->current_frame_i], 0, nullptr);

    vkCmdDraw(cmd_buf, 4, 1, 0, 0);
}

void renderer_draw(Renderer* renderer, std::vector<Entity>& entities, const WorldData* scene_data, EngineFeatures engine_features) {
    auto start_frame_time = system_clock::now();

    update_curr_frame_idx(renderer);
    Frame*          current_frame = &renderer->frames[renderer->current_frame_i];
    VkCommandBuffer cmd_buffer    = current_frame->command_context.primary_buffer;

    vkWaitForFences(renderer->vk_ctx->logical_device, 1, &current_frame->render_fence, VK_TRUE, vk_opts::timeout_dur);

    set_scene_data(renderer, scene_data);

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(renderer->vk_ctx->logical_device, renderer->swapchain_context.swapchain, vk_opts::timeout_dur,
                                            current_frame->present_semaphore, nullptr, &swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize(renderer);
        return;
    }

    VK_CHECK(vkResetFences(renderer->vk_ctx->logical_device, 1, &current_frame->render_fence));

    VkImage swapchain_image = renderer->swapchain_context.images[swapchain_image_index];

    command_ctx_begin_primary_buffer(&current_frame->command_context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // clear the entity id storage image
    vk_image_memory_barrier_insert(cmd_buffer, renderer->entity_id_images[renderer->current_frame_i].image, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkClearValue            entity_id_clear_value = {.color = {{0, 0, 0, 0}}};
    VkImageSubresourceRange range                 = vk_image_subresource_range_create(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0);

    vkCmdClearColorImage(cmd_buffer, renderer->entity_id_images[renderer->current_frame_i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &entity_id_clear_value.color, 1, &range);

    vk_image_memory_barrier_insert(cmd_buffer, renderer->entity_id_images[renderer->current_frame_i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_GENERAL);

    if (renderer->msaa_samples > 1) {
        vk_image_memory_barrier_insert(cmd_buffer, renderer->color_msaa_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    vk_image_memory_barrier_insert(cmd_buffer, renderer->color_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, renderer->depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd_buffer, &renderer->scene_rendering_info);

    set_render_state(renderer, cmd_buffer);

    if ((engine_features & EngineFeatures::SKY_BOX) == EngineFeatures::SKY_BOX) {
        render_sky_box(renderer, cmd_buffer);
    }

    render_geometry(renderer, cmd_buffer, entities);

    if ((engine_features & EngineFeatures::DEBUG_GRID) == EngineFeatures::DEBUG_GRID) {
        render_grid(renderer, cmd_buffer);
    }

    // render_cursor(renderer, cmd_buffer);

    vkCmdEndRendering(cmd_buffer);

    if (renderer->mode == EngineMode::EDIT) {
        // save image for UI viewport rendering into a dedicated image
        vk_image_memory_barrier_insert(cmd_buffer, renderer->color_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImage curr_viewport_img = renderer->viewport_images[renderer->current_frame_i].image;
        vk_image_memory_barrier_insert(cmd_buffer, curr_viewport_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vk_image_blit(cmd_buffer, renderer->color_image.image, curr_viewport_img, renderer->swapchain_context.extent, renderer->image_extent);

        vk_image_memory_barrier_insert(cmd_buffer, curr_viewport_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vk_image_memory_barrier_insert(cmd_buffer, renderer->color_image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        vkCmdBeginRendering(cmd_buffer, &renderer->ui_rendering_info);

        render_ui(cmd_buffer);

        vkCmdEndRendering(cmd_buffer);
    }

    vk_image_memory_barrier_insert(cmd_buffer, renderer->color_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_image_blit(cmd_buffer, renderer->color_image.image, swapchain_image, renderer->swapchain_context.extent, renderer->image_extent);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkSemaphoreSubmitInfo wait_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSemaphoreSubmitInfo signal_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    command_ctx_submit_primary_buffer(&current_frame->command_context, renderer->vk_ctx->queues.graphics, current_frame->render_fence,
                                      &wait_semaphore_si, &signal_semaphore_si);

    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.pSwapchains        = &renderer->swapchain_context.swapchain;
    present_info.swapchainCount     = 1;
    present_info.pImageIndices      = &swapchain_image_index;
    present_info.pWaitSemaphores    = &current_frame->render_semaphore;
    present_info.waitSemaphoreCount = 1;

    result = vkQueuePresentKHR(renderer->vk_ctx->queues.present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize(renderer);
        return;
    }

    renderer->frame_num++;

    auto end_time = system_clock::now();
    auto dur      = duration<float>(end_time - start_frame_time);
    renderer->stats.total_fps += 1000000.f / static_cast<float>(duration_cast<microseconds>(dur).count());
    renderer->stats.total_frame_time += duration_cast<microseconds>(dur).count();
    if (renderer->frame_num % 60 == 0) {
        renderer->stats.frame_time = duration_cast<microseconds>(dur).count() / 1000.f;
    }
}

void create_render_images(Renderer* renderer) {
    renderer->color_image = allocated_image_create(renderer->vk_ctx->logical_device, renderer->allocator,
                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                   VK_IMAGE_VIEW_TYPE_2D, renderer->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);

    if (renderer->msaa_samples > 1) {
        renderer->color_msaa_image = allocated_image_create(
            renderer->vk_ctx->logical_device, renderer->allocator, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            VK_IMAGE_VIEW_TYPE_2D, renderer->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, renderer->msaa_samples);
    }

    renderer->depth_image = allocated_image_create(renderer->vk_ctx->logical_device, renderer->allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                   VK_IMAGE_VIEW_TYPE_2D, renderer->image_extent, VK_FORMAT_D32_SFLOAT, 1, renderer->msaa_samples);

    if (renderer->mode == EngineMode::EDIT) {
        if (renderer->viewport_images.size() != renderer->swapchain_context.images.size()) {
            renderer->viewport_images.resize(renderer->swapchain_context.images.size());
        }
        for (size_t i = 0; i < renderer->swapchain_context.images.size(); i++) {
            AllocatedImage new_viewport_image = allocated_image_create(
                renderer->vk_ctx->logical_device, renderer->allocator, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_VIEW_TYPE_2D, renderer->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1, 1);
            renderer->viewport_images[i] = new_viewport_image;
        }
    }

    for (size_t i = 0; i < renderer->entity_id_images.size(); i++) {

        renderer->entity_id_images[i] = allocated_image_create(
            renderer->vk_ctx->logical_device, renderer->allocator, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_VIEW_TYPE_2D, renderer->image_extent, VK_FORMAT_R16G16_UINT, 1, 1, VMA_MEMORY_USAGE_AUTO);

        DescriptorWriter writer{};
        desc_writer_write_image_desc(&writer, 1, renderer->entity_id_images[i].image_view, renderer->default_linear_sampler, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        desc_writer_update_desc_set(&writer, renderer->vk_ctx->logical_device, renderer->scene_desc_sets[i]);
    }
}

void resize(Renderer* renderer) {
    vkDeviceWaitIdle(renderer->vk_ctx->logical_device);

    swapchain_ctx_reset(&renderer->swapchain_context, renderer->vk_ctx);

    allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->depth_image);
    allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->color_msaa_image);

    renderer->image_extent.width  = renderer->swapchain_context.extent.width;
    renderer->image_extent.height = renderer->swapchain_context.extent.height;

    renderer->render_area.scissor_dimensions.x += renderer->image_extent.width - renderer->render_area.scissor_dimensions.x;
    renderer->render_area.scissor_dimensions.y += renderer->image_extent.height - renderer->render_area.scissor_dimensions.y;

    allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->color_image);

    for (size_t i = 0; i < renderer->entity_id_images.size(); i++) {
        allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->entity_id_images[i]);
    }

    if (renderer->mode == EngineMode::EDIT) {
        for (size_t i = 0; i < renderer->swapchain_context.images.size(); i++) {
            allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->viewport_images[i]);
        }
    }

    create_render_images(renderer);

    for (size_t j = 0; j < renderer->viewport_images.size(); j++) {
        VkDescriptorSet new_viewport_ds = ImGui_ImplVulkan_AddTexture(renderer->default_linear_sampler, renderer->viewport_images[j].image_view,
                                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        renderer->viewport_desc_sets[j] = new_viewport_ds;
    }

    configure_render_resources(renderer);

    for (Frame& frame : renderer->frames) {
        frame_reset_synchronization(&frame, renderer->vk_ctx->logical_device);
    }
}

void render_geometry(Renderer* renderer, VkCommandBuffer cmd_buf, std::vector<Entity>& entities) {
    auto buffer_recording_start = system_clock::now();

    const VkDescriptorSet desc_sets[2] = {renderer->scene_desc_sets[renderer->current_frame_i], renderer->graphics_desc_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->geometry_pipeline_layout, 0, 2, desc_sets, 0, nullptr);

    const auto record_obj = [&](const DrawObject* obj) {
        vkCmdPushConstants(cmd_buf, renderer->geometry_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(MeshData), &obj->mesh_data);

        vkCmdBindIndexBuffer(cmd_buf, obj->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd_buf, obj->indices_count, 1, obj->indices_start, 0, 0);
    };

    const uint16_t geometry_vert = renderer->shader_indices.geometry_vert;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.vert_shaders[geometry_vert].stage,
                                          &renderer->shader_ctx.vert_shaders[geometry_vert].shader);

    const uint16_t geometry_frag = renderer->shader_indices.geometry_frag;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.frag_shaders[geometry_frag].stage,
                                          &renderer->shader_ctx.frag_shaders[geometry_frag].shader);

    renderer->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    vkCmdSetDepthTestEnable(cmd_buf, VK_TRUE);

    vkCmdSetDepthWriteEnable(cmd_buf, VK_TRUE);

    for (const auto& entity : entities) {

        for (const DrawObject& obj : entity.opaque_objs) {
            record_obj(&obj);
        }
    }

    for (const auto& entity : entities) {
        VkColorBlendEquationEXT blend_equation = {};

        // blend_equation = {
        //     .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        //     .dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
        //     .colorBlendOp        = VK_BLEND_OP_ADD,
        //     .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        //     .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        //     .alphaBlendOp        = VK_BLEND_OP_ADD,
        // };

        blend_equation = {
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
        };
        renderer->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &blend_equation);

        VkBool32 color_blend_enabled[] = {VK_TRUE};
        renderer->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enabled);

        for (const DrawObject& obj : entity.transparent_objs) {
            record_obj(&obj);
        }
    }

    auto end_time = system_clock::now();
    auto dur      = duration<float>(end_time - buffer_recording_start);
    renderer->stats.total_draw_time += static_cast<uint32_t>(duration_cast<microseconds>(dur).count());
    if (renderer->frame_num % 60 == 0) {
        renderer->stats.draw_time = duration_cast<microseconds>(dur).count();
    }
}

void render_ui(VkCommandBuffer cmd_buf) { ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf); }

void render_grid(const Renderer* renderer, VkCommandBuffer cmd_buf) {

    VkColorBlendEquationEXT blend_equation = {};

    blend_equation = {
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
    };
    renderer->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &blend_equation);

    VkBool32 color_blend_enabled[] = {VK_TRUE};
    renderer->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enabled);

    const uint16_t grid_vert = renderer->shader_indices.grid_vert;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.vert_shaders[grid_vert].stage,
                                          &renderer->shader_ctx.vert_shaders[grid_vert].shader);

    const uint16_t grid_frag = renderer->shader_indices.grid_frag;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.frag_shaders[grid_frag].stage,
                                          &renderer->shader_ctx.frag_shaders[grid_frag].shader);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->grid_pipeline_layout, 0, 1,
                            &renderer->scene_desc_sets[renderer->current_frame_i], 0, nullptr);

    vkCmdDraw(cmd_buf, 6, 1, 0, 0);
}

void set_render_state(Renderer* renderer, VkCommandBuffer cmd_buf) {

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = renderer->image_extent.width;
    viewport.height   = renderer->image_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D scissor_extent{};
    scissor_extent.width  = renderer->render_area.scissor_dimensions.x;
    scissor_extent.height = renderer->render_area.scissor_dimensions.y;

    VkOffset2D scissor_offset{};
    scissor_offset.x = renderer->render_area.top_left.x;
    scissor_offset.y = renderer->render_area.top_left.y;

    VkRect2D scissor{};
    scissor.extent = scissor_extent;
    scissor.offset = scissor_offset;

    vkCmdSetViewportWithCount(cmd_buf, 1, &viewport);

    vkCmdSetScissorWithCount(cmd_buf, 1, &scissor);

    vkCmdSetRasterizerDiscardEnable(cmd_buf, VK_FALSE);

    vkCmdSetCullMode(cmd_buf, VK_CULL_MODE_NONE);

    renderer->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    VkColorBlendEquationEXT colorBlendEquationEXT{};
    renderer->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &colorBlendEquationEXT);

    VkBool32 color_blend_enables[] = {VK_FALSE};
    renderer->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enables);

    vkCmdSetPrimitiveTopology(cmd_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vkCmdSetPrimitiveRestartEnable(cmd_buf, VK_FALSE);

    renderer->ext_ctx.vkCmdSetRasterizationSamplesEXT(cmd_buf, static_cast<VkSampleCountFlagBits>(renderer->msaa_samples));

    constexpr uint32_t max = ~0;

    constexpr VkSampleMask sample_masks[4] = {max, max, max, max};

    renderer->ext_ctx.vkCmdSetSampleMaskEXT(cmd_buf, VK_SAMPLE_COUNT_1_BIT, sample_masks);

    renderer->ext_ctx.vkCmdSetAlphaToCoverageEnableEXT(cmd_buf, VK_FALSE);

    renderer->ext_ctx.vkCmdSetPolygonModeEXT(cmd_buf, VK_POLYGON_MODE_FILL);

    renderer->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    renderer->ext_ctx.vkCmdSetTessellationDomainOriginEXT(cmd_buf, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);

    renderer->ext_ctx.vkCmdSetPatchControlPointsEXT(cmd_buf, 1);

    vkCmdSetLineWidth(cmd_buf, 1.0f);
    vkCmdSetFrontFace(cmd_buf, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthCompareOp(cmd_buf, VK_COMPARE_OP_GREATER_OR_EQUAL);
    vkCmdSetDepthTestEnable(cmd_buf, VK_FALSE);
    vkCmdSetDepthWriteEnable(cmd_buf, VK_TRUE);
    vkCmdSetDepthBoundsTestEnable(cmd_buf, VK_FALSE);
    vkCmdSetDepthBiasEnable(cmd_buf, VK_FALSE);
    vkCmdSetStencilTestEnable(cmd_buf, VK_FALSE);

    renderer->ext_ctx.vkCmdSetLogicOpEnableEXT(cmd_buf, VK_FALSE);

    VkColorComponentFlags color_component_flags[] = {VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT};

    renderer->ext_ctx.vkCmdSetColorWriteMaskEXT(cmd_buf, 0, 1, color_component_flags);

    // set default bindings (null) to all shader types for the graphics bind point
    // https://docs.vulkan.org/spec/latest/chapters/shaders.html#shaders-binding
    // constexpr std::array graphics_pipeline_stages = {
    //     VK_SHADER_STAGE_VERTEX_BIT,
    //     VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    //     VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    //     VK_SHADER_STAGE_GEOMETRY_BIT,
    //     VK_SHADER_STAGE_FRAGMENT_BIT,
    //     VK_SHADER_STAGE_TASK_BIT_EXT,
    //     VK_SHADER_STAGE_MESH_BIT_EXT,
    // };

    //    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, graphics_pipeline_stages.size(), graphics_pipeline_stages.data(), VK_NULL_HANDLE);
}

void upload_sky_box_texture(Renderer* renderer, const TextureSampler* tex_sampler) {

    assert(tex_sampler->layer_count == 6 && tex_sampler->view_type == VK_IMAGE_VIEW_TYPE_CUBE);

    const VkExtent2D extent{.width = tex_sampler->width, .height = tex_sampler->height};

    const uint32_t byte_size = tex_sampler->width * tex_sampler->height * tex_sampler->color_channels * tex_sampler->layer_count;

    AllocatedBuffer staging_buf = allocated_buffer_create(renderer->allocator, byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(renderer->allocator, tex_sampler->mip_levels[0].data, staging_buf.allocation, 0, byte_size);

    const AllocatedImage new_texture =
        allocated_image_create(renderer->vk_ctx->logical_device, renderer->allocator, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               tex_sampler->view_type, extent, VK_FORMAT_R8G8B8A8_SRGB, 1);

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

    command_ctx_immediate_submit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                 renderer->imm_fence, [&](VkCommandBuffer cmd) {
                                     vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 0, tex_sampler->layer_count);

                                     vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                            copy_regions.size(), copy_regions.data());

                                     vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, tex_sampler->layer_count);
                                 });

    DescriptorWriter descriptor_writer;
    desc_writer_write_image_desc(&descriptor_writer, renderer->sky_box_desc_binding, new_texture.image_view, tex_sampler->sampler,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    desc_writer_update_desc_set(&descriptor_writer, renderer->vk_ctx->logical_device, renderer->graphics_desc_set);

    allocated_buffer_destroy(renderer->allocator, &staging_buf);
    renderer->sky_box_image = new_texture;

    renderer->deletion_queue.push_persistent(
        [=] { allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->sky_box_image); });
}

struct UniqueImageInstance {
    uint32_t              color_channels{};
    uint32_t              height{};
    uint32_t              width{};
    uint32_t              byte_size{};
    VkFormat              format{};
    std::vector<MipLevel> mip_levels;
};

void generate_mip_maps(Renderer* renderer, AllocatedImage* allocated_img, VkCommandBuffer cmd_buf) {
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

uint32_t renderer_upload_2d_textures(Renderer* renderer, std::vector<TextureSampler>& tex_samplers, uint32_t color_channels) {

    // we will return this at the end of the function. It signifies an offset for
    // materials accessing these textures by their index.
    // For instance, if a gltf mesh is trying to access texture index 3, and this function passes
    // back the number 5, then the mesh should point to texture index 8 since this is where the
    // descriptor for this texture will actually be found in the shader
    const uint32_t descriptor_index_offset = renderer->tex_sampler_desc_count;

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
        new_image.format     = tex_sampler.format;

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
    AllocatedBuffer staging_buf = allocated_buffer_create(renderer->allocator, largest_image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    DescriptorWriter descriptor_writer{};
    for (size_t i = 0; i < unique_image_instances.size(); i++) {
        const UniqueImageInstance* image = &unique_image_instances[i];

        VkExtent2D extent = {
            .width  = image->width,
            .height = image->height,
        };

        AllocatedImage tex_image =
            allocated_image_create(renderer->vk_ctx->logical_device, renderer->allocator,
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_IMAGE_VIEW_TYPE_2D, extent, image->format, image->mip_levels.size());

        command_ctx_immediate_submit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                     renderer->imm_fence, [&](VkCommandBuffer cmd) {
                                         // transition all mips to transfer destination
                                         vk_image_memory_barrier_insert(cmd, tex_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex_image.mip_levels);
                                         for (size_t mip = 0; mip < image->mip_levels.size(); mip++) {
                                             const MipLevel* mip_level = &image->mip_levels[mip];
                                             // copy this mip to staging
                                             vmaCopyMemoryToAllocation(renderer->allocator, image->mip_levels[mip].data, staging_buf.allocation, 0,
                                                                       mip_level->height * mip_level->width * color_channels);

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

        renderer->tex_images.push_back(tex_image);
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
        uint32_t tex_img_i = renderer->tex_images.size() - unique_image_instances.size() + image_i;

        const AllocatedImage* tex_image = &renderer->tex_images[tex_img_i];

        // renderer->mip_mapped_samplers.push_back(
        //     vk_sampler_create(renderer->vk_ctx->logical_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, 0.f, tex_image->mip_levels));

        desc_writer_write_image_desc(&descriptor_writer, renderer->texture_desc_binding, tex_image->image_view, renderer->default_linear_sampler,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     renderer->tex_sampler_desc_count++);
        desc_writer_update_desc_set(&descriptor_writer, renderer->vk_ctx->logical_device, renderer->graphics_desc_set);
        desc_writer_clear(&descriptor_writer);
    }

    // for (auto& data : compressed_data_bufs) {
    //     // free(data);
    //     free(data.data);
    // }
    allocated_buffer_destroy(renderer->allocator, &staging_buf);

    return descriptor_index_offset;
}

void render_sky_box(const Renderer* renderer, VkCommandBuffer cmd_buf) {

    vkCmdSetDepthWriteEnable(cmd_buf, VK_FALSE);

    const uint16_t sky_box_vert = renderer->shader_indices.sky_box_vert;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.vert_shaders[sky_box_vert].stage,
                                          &renderer->shader_ctx.vert_shaders[sky_box_vert].shader);

    const uint16_t sky_box_frag = renderer->shader_indices.sky_box_frag;
    renderer->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &renderer->shader_ctx.frag_shaders[sky_box_frag].stage,
                                          &renderer->shader_ctx.frag_shaders[sky_box_frag].shader);

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

    renderer->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 1, &input_description, 1, &attribute_description);

    const VkDescriptorSet desc_sets[2] = {renderer->scene_desc_sets[renderer->current_frame_i], renderer->graphics_desc_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->sky_box_pipeline_layout, 0, 2, desc_sets, 0, nullptr);

    VkDeviceSize offsets = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &renderer->sky_box_vert_buffer.buffer, &offsets);

    vkCmdDraw(cmd_buf, 36, 1, 0, 0);
}

VkShaderModule load_shader_module(const Renderer* renderer, std::span<uint32_t> shader_spv) {

    VkShaderModule           shader_module;
    VkShaderModuleCreateInfo shader_module_ci{};
    shader_module_ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_ci.codeSize = shader_spv.size() * sizeof(uint32_t);
    shader_module_ci.pCode    = shader_spv.data();

    VK_CHECK(vkCreateShaderModule(renderer->vk_ctx->logical_device, &shader_module_ci, nullptr, &shader_module));

    return shader_module;
}

void renderer_create_accel_struct(Renderer* renderer, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                  std::span<const TopLevelInstanceRef> instance_refs) {
    accel_struct_ctx_add_triangles_geometry(&renderer->accel_struct_ctx, renderer->vk_ctx->logical_device, renderer->allocator, &renderer->ext_ctx,
                                            renderer->vk_ctx->queues.graphics, bottom_level_geometries, instance_refs);

    DescriptorWriter desc_writer;
    desc_writer_write_accel_struct_desc(&desc_writer, renderer->accel_struct_desc_binding, &renderer->accel_struct_ctx.top_level,
                                        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
    desc_writer_update_desc_set(&desc_writer, renderer->vk_ctx->logical_device, renderer->graphics_desc_set);
};

void renderer_update_accel_struct(Renderer* renderer, const glm::mat4* transform, uint32_t instance_idx) {
    accel_struct_ctx_update_tlas(&renderer->accel_struct_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device, renderer->vk_ctx->queues.graphics,
                                 renderer->allocator, transform, instance_idx);
}

void renderer_finish_pending_vk_work(const Renderer* renderer) { vkDeviceWaitIdle(renderer->vk_ctx->logical_device); }

void renderer_deinit(Renderer* renderer) {
    DEBUG_PRINT("destroying Vulkan renderer");

    fmt::println("average renderer_draw time: {:.3f} us",
                 static_cast<float>(renderer->stats.total_draw_time) / static_cast<float>(renderer->frame_num));
    fmt::println("average frame time: {:.3f} ms",
                 static_cast<float>(renderer->stats.total_frame_time) / 1000.f / static_cast<float>(renderer->frame_num));
    fmt::println("average fps: {:.3f}", static_cast<float>(renderer->stats.total_fps) / static_cast<float>(renderer->frame_num));

    renderer->deletion_queue.flush();

    for (Frame& frame : renderer->frames) {
        frame_deinit(&frame, renderer->vk_ctx->logical_device);
    }

    command_ctx_deinit(&renderer->immediate_cmd_ctx, renderer->vk_ctx->logical_device);
    command_ctx_deinit(&renderer->compute_cmd_ctx, renderer->vk_ctx->logical_device);

    shader_ctx_deinit(&renderer->shader_ctx, &renderer->ext_ctx, renderer->vk_ctx->logical_device);

    accel_struct_ctx_deinit(&renderer->accel_struct_ctx, &renderer->ext_ctx, renderer->allocator, renderer->vk_ctx->logical_device);

    swapchain_ctx_deinit(&renderer->swapchain_context, renderer->vk_ctx);

    desc_allocator_deinit(&renderer->scene_desc_allocator, renderer->vk_ctx->logical_device);
    desc_allocator_deinit(&renderer->graphics_desc_allocator, renderer->vk_ctx->logical_device);

    for (const auto& tex_image : renderer->tex_images) {
        allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &tex_image);
    }

    if (renderer->msaa_samples > 1) {
        allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->color_msaa_image);
    }
    allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->depth_image);
    allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &renderer->color_image);

    if (renderer->mode == EngineMode::EDIT) {
        for (const auto& image : renderer->viewport_images) {
            allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &image);
        }
    }

    for (const auto& entity_id_img : renderer->entity_id_images) {
        allocated_image_destroy(renderer->vk_ctx->logical_device, renderer->allocator, &entity_id_img);
    }

    for (const auto& scene_buf : renderer->scene_data_buffers) {
        allocated_buffer_destroy(renderer->allocator, &scene_buf);
    }

    allocated_buffer_destroy(renderer->allocator, &renderer->mat_buffer);
    allocated_buffer_destroy(renderer->allocator, &renderer->entity_id_result_buffer);

    vkDestroySampler(renderer->vk_ctx->logical_device, renderer->default_nearest_sampler, nullptr);
    vkDestroySampler(renderer->vk_ctx->logical_device, renderer->default_linear_sampler, nullptr);

    vkDestroyPipelineLayout(renderer->vk_ctx->logical_device, renderer->grid_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(renderer->vk_ctx->logical_device, renderer->geometry_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(renderer->vk_ctx->logical_device, renderer->sky_box_pipeline_layout, nullptr);

    vkDestroyDescriptorPool(renderer->vk_ctx->logical_device, renderer->imm_descriptor_pool, nullptr);

    vkDestroyDescriptorSetLayout(renderer->vk_ctx->logical_device, renderer->scene_desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->vk_ctx->logical_device, renderer->graphics_desc_set_layout, nullptr);
    vkDestroyFence(renderer->vk_ctx->logical_device, renderer->imm_fence, nullptr);

    vmaDestroyAllocator(renderer->allocator);
}