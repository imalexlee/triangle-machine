
#include <array>
#include <iostream>
#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "global_utils.h"
#include "imgui.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_pipeline.h"
#include "vk_init.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <vk_backend/vk_command.h>
#include <vk_backend/vk_debug.h>
#include <vk_backend/vk_device.h>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>
#include <vk_backend/vk_utils.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "vk_backend/vk_backend.h"

#include "imgui_impl_vulkan.h"
#include "vk_backend/vk_sync.h"
#include "vk_options.h"

#include <set>

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
static void set_scene_data(const VkBackend* backend, const SceneData* scene_data);
// rendering
static void render_geometry(VkBackend* backend, VkCommandBuffer cmd_buf, std::span<const Entity> entities, size_t vert_shader, size_t frag_shader);
static void render_ui(VkCommandBuffer cmd_buf);
static void render_grid(const VkBackend* backend, VkCommandBuffer cmd_buf);
static void render_sky_box(const VkBackend* backend, VkCommandBuffer cmd_buf, uint32_t vert_shader_i, uint32_t frag_shader_i);

static uint32_t                 get_curr_frame_idx(const VkBackend* backend) { return backend->frame_num % backend->frames.size(); }
static std::vector<const char*> get_instance_extensions();

using namespace std::chrono;

static VkBackend* active_backend = nullptr;

void backend_init(VkBackend* backend, VkInstance instance, VkSurfaceKHR surface, uint32_t width, uint32_t height) {

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

    for (size_t i = 0; i < backend->frames.size(); i++) {
        Frame* frame = &backend->frames[i];
        frame_init(frame, backend->device_ctx.logical_device, backend->allocator, backend->device_ctx.queues.graphics_family_index);
        backend->scene_data_buffers[i] =
            allocated_buffer_create(backend->allocator, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    backend->image_extent.width  = width;
    backend->image_extent.height = height;

    backend->color_resolve_image = allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                          VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    backend->color_image = allocated_image_create(
        backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, backend->device_ctx.raster_samples);

    backend->depth_image =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_D32_SFLOAT, backend->device_ctx.raster_samples);

    // create color attachments and  rendering information from our allocated images
    configure_render_resources(backend);

    backend->imm_fence = vk_fence_create(backend->device_ctx.logical_device, VK_FENCE_CREATE_SIGNALED_BIT);

    command_ctx_init(&backend->immediate_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.graphics_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    command_ctx_init(&backend->compute_cmd_ctx, backend->device_ctx.logical_device, backend->device_ctx.queues.compute_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    create_grid_pipeline(backend);

    create_pipeline_layouts(backend);

    create_compute_resources(backend);

    create_default_data(backend);

    shader_ctx_init(&backend->shader_ctx);

    ext_context_init(&backend->ext_ctx, backend->device_ctx.logical_device);

    if constexpr (vk_opts::validation_enabled) {
        configure_debugger(backend);
    }
}

static void create_compute_resources(VkBackend* backend) {
    // TODO: create resources
}

void set_scene_data(const VkBackend* backend, const SceneData* scene_data) {
    uint32_t               curr_frame_idx      = get_curr_frame_idx(backend);
    VkDescriptorSet        curr_scene_desc_set = backend->scene_desc_sets[curr_frame_idx];
    const AllocatedBuffer* curr_scene_buf      = &backend->scene_data_buffers[curr_frame_idx];

    vmaCopyMemoryToAllocation(backend->allocator, scene_data, curr_scene_buf->allocation, 0, sizeof(SceneData));

    DescriptorWriter desc_writer;
    desc_writer_write_buffer_desc(&desc_writer, 0, curr_scene_buf->buffer, sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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
    sky_box_tex_sampler.data      = texture_data;
    sky_box_tex_sampler.sampler   = backend->default_linear_sampler;
    sky_box_tex_sampler.view_type = VK_IMAGE_VIEW_TYPE_CUBE;

    sky_box_tex_sampler.width          = width;
    sky_box_tex_sampler.height         = height;
    sky_box_tex_sampler.color_channels = color_channels;
    sky_box_tex_sampler.layer_count    = 6;

    upload_sky_box_texture(backend, &sky_box_tex_sampler);
}

void create_pipeline_layouts(VkBackend* backend) {

    std::array set_layouts = {backend->scene_desc_set_layout, backend->graphics_desc_set_layout};

    std::array<VkPushConstantRange, 1> push_constant_ranges = {{{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(MeshData),
    }}};

    backend->geometry_pipeline_layout = vk_pipeline_layout_create(backend->device_ctx.logical_device, set_layouts, push_constant_ranges, 0);

    backend->sky_box_pipeline_layout = vk_pipeline_layout_create(backend->device_ctx.logical_device, set_layouts, {}, 0);
}

static void create_grid_pipeline(VkBackend* backend) {
    PipelineBuilder pb;

    std::ifstream         v_file("../shaders/vertex/grid.vert.spv", std::ios::ate | std::ios::binary);
    size_t                v_file_size = v_file.tellg();
    std::vector<uint32_t> v_buf(v_file_size / sizeof(uint32_t));

    v_file.seekg(0);
    v_file.read(reinterpret_cast<char*>(v_buf.data()), v_file_size);
    v_file.close();

    std::ifstream         f_file("../shaders/fragment/grid.frag.spv", std::ios::ate | std::ios::binary);
    size_t                f_file_size = f_file.tellg();
    std::vector<uint32_t> f_buf(f_file_size / sizeof(uint32_t));

    f_file.seekg(0);
    f_file.read(reinterpret_cast<char*>(f_buf.data()), f_file_size);
    f_file.close();

    const VkShaderModule vert_shader = load_shader_module(backend, v_buf);
    const VkShaderModule frag_shader = load_shader_module(backend, f_buf);

    std::array                         set_layouts{backend->scene_desc_set_layout};
    std::array<VkPushConstantRange, 0> push_constant_ranges{{}};

    pipeline_builder_set_shaders(&pb, vert_shader, frag_shader);
    pipeline_builder_set_topology(&pb, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_raster_state(&pb, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_multisampling(&pb, static_cast<VkSampleCountFlagBits>(backend->device_ctx.raster_samples));
    pipeline_builder_set_depth_state(&pb, true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder_set_render_state(&pb, backend->color_image.image_format, backend->depth_image.image_format);
    pipeline_builder_set_blending(&pb, BlendMode::alpha);
    pipeline_builder_set_layout(&pb, set_layouts, push_constant_ranges, 0);

    backend->grid_pipeline_info = pipeline_builder_create_pipeline(&pb, backend->device_ctx.logical_device);

    backend->deletion_queue.push_persistent([=] {
        vkDestroyShaderModule(backend->device_ctx.logical_device, vert_shader, nullptr);
        vkDestroyShaderModule(backend->device_ctx.logical_device, frag_shader, nullptr);
    });
}

// adding names to these 64 bit handles helps a lot when reading validation errors
void configure_debugger(VkBackend* backend) {
    debugger_init(&backend->debugger, backend->instance, backend->device_ctx.logical_device);
    debugger_set_handle_name(&backend->debugger, backend->color_image.image, VK_OBJECT_TYPE_IMAGE, "color image");
    debugger_set_handle_name(&backend->debugger, backend->depth_image.image, VK_OBJECT_TYPE_IMAGE, "depth image");
    debugger_set_handle_name(&backend->debugger, backend->color_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color image view");
    debugger_set_handle_name(&backend->debugger, backend->depth_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "depth image view");
    debugger_set_handle_name(&backend->debugger, backend->color_resolve_image.image, VK_OBJECT_TYPE_IMAGE, "color resolve image");
    debugger_set_handle_name(&backend->debugger, backend->color_resolve_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color resolve image view");
    debugger_set_handle_name(&backend->debugger, backend->immediate_cmd_ctx.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, "imm cmd_buf buf");
    debugger_set_handle_name(&backend->debugger, backend->sky_box_vert_buffer.buffer, VK_OBJECT_TYPE_BUFFER, "skybox buffer");

    for (size_t i = 0; i < backend->frames.size(); i++) {
        const Frame& frame = backend->frames[i];
        debugger_set_handle_name(&backend->debugger, frame.command_context.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                                 "frame " + std::to_string(i) + " cmd_buf buf");
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
    backend->scene_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, backend->device_ctx.logical_device);

    desc_layout_builder_clear(&layout_builder);

    // materials
    desc_layout_builder_add_binding(&layout_builder, backend->material_desc_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // sky box
    desc_layout_builder_add_binding(&layout_builder, backend->sky_box_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // textures
    constexpr VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                                       VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

    desc_layout_builder_add_binding(&layout_builder, backend->texture_desc_binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT, binding_flags, 300);

    backend->graphics_desc_set_layout = desc_layout_builder_create_layout(&layout_builder, backend->device_ctx.logical_device);
}

// creates a scene desc set per frame
void create_scene_desc_sets(VkBackend* backend) {
    std::array<PoolSizeRatio, 1> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };
    desc_allocator_init(&backend->scene_desc_allocator, backend->device_ctx.logical_device, 3, pool_sizes);

    for (size_t i = 0; i < backend->scene_desc_sets.size(); i++) {
        backend->scene_desc_sets[i] =
            desc_allocator_allocate_desc_set(&backend->scene_desc_allocator, backend->device_ctx.logical_device, backend->scene_desc_set_layout);
    }
}

void create_graphics_desc_set(VkBackend* backend) {
    std::array<PoolSizeRatio, 3> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 300}}
    };
    desc_allocator_init(&backend->graphics_desc_allocator, backend->device_ctx.logical_device, 1, pool_sizes);

    backend->graphics_desc_set = desc_allocator_allocate_desc_set(&backend->graphics_desc_allocator, backend->device_ctx.logical_device,
                                                                  backend->graphics_desc_set_layout, 300);
}

void configure_render_resources(VkBackend* backend) {

    backend->scene_clear_value = {.color = {{0.1f, 0.1f, 0.1f, 0.2f}}};

    backend->scene_color_attachment =
        vk_color_attachment_info_create(backend->color_image.image_view, &backend->scene_clear_value, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                        VK_ATTACHMENT_STORE_OP_DONT_CARE, backend->color_resolve_image.image_view);

    backend->scene_depth_attachment =
        vk_depth_attachment_info_create(backend->depth_image.image_view, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

    backend->scene_rendering_info =
        vk_rendering_info_create(&backend->scene_color_attachment, &backend->scene_depth_attachment, backend->image_extent);
}

void create_default_data(VkBackend* backend) {

    backend->stats.total_fps        = 0;
    backend->stats.total_frame_time = 0;
    backend->stats.total_draw_time  = 0;

    backend->default_linear_sampler = vk_sampler_create(backend->device_ctx.logical_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR);

    backend->default_nearest_sampler = vk_sampler_create(backend->device_ctx.logical_device, VK_FILTER_NEAREST, VK_FILTER_NEAREST);

    uint32_t       white = 0xFFFFFFFF;
    TextureSampler default_tex_sampler{};
    default_tex_sampler.width          = 1;
    default_tex_sampler.height         = 1;
    default_tex_sampler.color_channels = 4;
    default_tex_sampler.layer_count    = 1;
    default_tex_sampler.data           = reinterpret_cast<const uint8_t*>(&white);
    default_tex_sampler.sampler        = backend->default_linear_sampler;
    default_tex_sampler.view_type      = VK_IMAGE_VIEW_TYPE_2D;

    TextureSampler tex_samplers[1] = {default_tex_sampler};

    // default texture will always be assumed to be at index 0
    std::ignore = backend_upload_2d_texture(backend, tex_samplers);
}

void backend_create_imgui_resources(VkBackend* backend) {

    std::array<VkDescriptorPoolSize, 1> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
    };

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.maxSets       = 1;
    pool_ci.pPoolSizes    = pool_sizes.data();
    pool_ci.poolSizeCount = pool_sizes.size();
    pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(backend->device_ctx.logical_device, &pool_ci, nullptr, &backend->imm_descriptor_pool));

    VkPipelineRenderingCreateInfoKHR pipeline_info{};
    pipeline_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_info.pColorAttachmentFormats = &backend->color_image.image_format;
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
    init_info.MSAASamples                 = static_cast<VkSampleCountFlagBits>(backend->device_ctx.raster_samples);

    ImGui_ImplVulkan_Init(&init_info);
}

void backend_immediate_submit(const VkBackend* backend, std::function<void(VkCommandBuffer cmd_buf)>&& function) {
    VK_CHECK(vkResetFences(backend->device_ctx.logical_device, 1, &backend->imm_fence));

    command_ctx_begin_primary_buffer(&backend->immediate_cmd_ctx, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(backend->immediate_cmd_ctx.primary_buffer);

    command_ctx_submit_primary_buffer(&backend->immediate_cmd_ctx, backend->device_ctx.queues.graphics, nullptr, nullptr, backend->imm_fence);

    VK_CHECK(vkWaitForFences(backend->device_ctx.logical_device, 1, &backend->imm_fence, VK_TRUE, vk_opts::timeout_dur));
}

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

void backend_upload_vert_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array<VkPushConstantRange, 1> push_constant_ranges{{{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(MeshData),
    }}};

    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_VERTEX_BIT,
                            VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);
}

void backend_upload_frag_shader(VkBackend* backend, const std::filesystem::path& file_path, const std::string& name) {
    ShaderBuilder builder;

    std::array<VkPushConstantRange, 1> push_constant_ranges{{{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(MeshData),
    }}};

    std::array set_layouts{
        backend->scene_desc_set_layout,
        backend->graphics_desc_set_layout,
    };

    shader_ctx_stage_shader(&backend->shader_ctx, file_path, name, set_layouts, push_constant_ranges, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::unlinked);
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

    shader_ctx_commit_shaders(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device, ShaderType::linked);
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

void backend_draw(VkBackend* backend, std::span<const Entity> entities, const SceneData* scene_data, size_t vert_shader, size_t frag_shader) {
    auto start_frame_time = system_clock::now();

    Frame*          current_frame = &backend->frames[get_curr_frame_idx(backend)];
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

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, backend->depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd_buffer, &backend->scene_rendering_info);

    set_render_state(backend, cmd_buffer);

    render_sky_box(backend, cmd_buffer, 1, 1);

    render_geometry(backend, cmd_buffer, entities, vert_shader, frag_shader);

    // render_grid(backend, cmd_buffer);

    render_ui(cmd_buffer);

    vkCmdEndRendering(cmd_buffer);

    vk_image_memory_barrier_insert(cmd_buffer, backend->color_resolve_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_image_blit(cmd_buffer, backend->color_resolve_image.image, swapchain_image, backend->swapchain_context.extent, backend->image_extent, 1, 1);

    vk_image_memory_barrier_insert(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkSemaphoreSubmitInfo wait_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSemaphoreSubmitInfo signal_semaphore_si =
        vk_semaphore_submit_info_create(current_frame->render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    command_ctx_submit_primary_buffer(&current_frame->command_context, backend->device_ctx.queues.graphics, &wait_semaphore_si, &signal_semaphore_si,
                                      current_frame->render_fence);

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

    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_resolve_image);
    backend->color_resolve_image = allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                          VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    backend->color_image = allocated_image_create(
        backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, backend->device_ctx.raster_samples);

    backend->depth_image =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_VIEW_TYPE_2D, backend->image_extent, VK_FORMAT_D32_SFLOAT, backend->device_ctx.raster_samples);

    configure_render_resources(backend);

    for (Frame& frame : backend->frames) {
        frame_reset_synchronization(&frame, backend->device_ctx.logical_device);
    }
}

void render_geometry(VkBackend* backend, VkCommandBuffer cmd_buf, std::span<const Entity> entities, size_t vert_shader, size_t frag_shader) {
    auto buffer_recording_start = system_clock::now();

    const VkDescriptorSet desc_sets[2] = {backend->scene_desc_sets[get_curr_frame_idx(backend)], backend->graphics_desc_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->geometry_pipeline_layout, 0, 2, desc_sets, 0, nullptr);

    const auto record_obj = [&](const DrawObject* obj) {
        vkCmdPushConstants(cmd_buf, backend->geometry_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshData),
                           &obj->mesh_data);

        vkCmdBindIndexBuffer(cmd_buf, obj->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd_buf, obj->indices_count, 1, obj->indices_start, 0, 0);
    };

    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[vert_shader].stage,
                                         &backend->shader_ctx.vert_shaders[vert_shader].shader);

    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[frag_shader].stage,
                                         &backend->shader_ctx.frag_shaders[frag_shader].shader);

    backend->ext_ctx.vkCmdSetPolygonModeEXT(cmd_buf, VK_POLYGON_MODE_FILL);

    backend->ext_ctx.vkCmdSetVertexInputEXT(cmd_buf, 0, nullptr, 0, nullptr);

    vkCmdSetDepthTestEnable(cmd_buf, VK_TRUE);

    vkCmdSetDepthWriteEnable(cmd_buf, VK_TRUE);

    for (const auto& entity : entities) {
        for (const DrawObject& obj : entity.opaque_objs) {
            record_obj(&obj);
        }
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

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = backend->image_extent.width;
    viewport.height   = backend->image_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = backend->image_extent;
    scissor.offset = {0, 0};

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->grid_pipeline_info.pipeline);

    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, backend->grid_pipeline_info.pipeline_layout, 0, 1,
                            &backend->scene_desc_sets[get_curr_frame_idx(backend)], 0, nullptr);

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

    VkRect2D scissor{};
    scissor.extent = backend->image_extent;
    scissor.offset = {0, 0};

    vkCmdSetViewportWithCount(cmd_buf, 1, &viewport);

    vkCmdSetScissorWithCount(cmd_buf, 1, &scissor);

    vkCmdSetRasterizerDiscardEnable(cmd_buf, VK_FALSE);

    VkColorBlendEquationEXT colorBlendEquationEXT{};
    backend->ext_ctx.vkCmdSetColorBlendEquationEXT(cmd_buf, 0, 1, &colorBlendEquationEXT);

    VkBool32 color_blend_enables[] = {VK_FALSE};
    backend->ext_ctx.vkCmdSetColorBlendEnableEXT(cmd_buf, 0, 1, color_blend_enables);

    vkCmdSetPrimitiveTopology(cmd_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vkCmdSetPrimitiveRestartEnable(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdSetRasterizationSamplesEXT(cmd_buf, static_cast<VkSampleCountFlagBits>(backend->device_ctx.raster_samples));

    uint32_t max = ~0;

    const VkSampleMask sample_masks[4] = {max, max, max, max};

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
    vkCmdSetDepthBoundsTestEnable(cmd_buf, VK_FALSE);
    vkCmdSetDepthBiasEnable(cmd_buf, VK_FALSE);
    vkCmdSetStencilTestEnable(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdSetLogicOpEnableEXT(cmd_buf, VK_FALSE);

    VkColorComponentFlags color_component_flags[] = {VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT};

    backend->ext_ctx.vkCmdSetColorWriteMaskEXT(cmd_buf, 0, 1, color_component_flags);
}

void upload_sky_box_texture(VkBackend* backend, const TextureSampler* tex_sampler) {

    assert(tex_sampler->layer_count == 6 && tex_sampler->view_type == VK_IMAGE_VIEW_TYPE_CUBE);

    const VkExtent2D extent{.width = tex_sampler->width, .height = tex_sampler->height};

    const uint32_t byte_size = tex_sampler->width * tex_sampler->height * tex_sampler->color_channels * tex_sampler->layer_count;

    AllocatedBuffer staging_buf = allocated_buffer_create(backend->allocator, byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    vmaCopyMemoryToAllocation(backend->allocator, tex_sampler->data, staging_buf.allocation, 0, byte_size);

    const AllocatedImage new_texture =
        allocated_image_create(backend->device_ctx.logical_device, backend->allocator, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               tex_sampler->view_type, extent, VK_FORMAT_R8G8B8A8_UNORM);

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

    backend_immediate_submit(backend, [&](VkCommandBuffer cmd) {
        vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       tex_sampler->layer_count);

        vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_regions.size(),
                               copy_regions.data());

        vk_image_memory_barrier_insert(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       tex_sampler->layer_count);
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

uint32_t backend_upload_2d_texture(VkBackend* backend, std::span<const TextureSampler> tex_samplers) {

    // we will return this at the end of the function. It signifies an offset for
    // materials accessing these textures by their index.
    // For instance, if a gltf mesh is trying to access texture index 3, and this function passes
    // back the number 5, then the mesh should point to texture index 8 since this is where the
    // descriptor for this texture will actually be found in the shader
    const uint32_t descriptor_index_offset = backend->tex_images.size();

    // find how much memory to allocate
    uint32_t total_byte_size = 0;
    for (const auto& tex_sampler : tex_samplers) {
        assert(tex_sampler.layer_count == 1 && tex_sampler.view_type == VK_IMAGE_VIEW_TYPE_2D);
        const uint32_t byte_size = tex_sampler.width * tex_sampler.height * tex_sampler.color_channels;
        total_byte_size += byte_size;
    }

    AllocatedBuffer staging_buf = allocated_buffer_create(backend->allocator, total_byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                          VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    // fill the staging buffer with the images
    uint32_t staging_buf_offset = 0;
    for (const auto& tex_sampler : tex_samplers) {
        const uint32_t byte_size = tex_sampler.width * tex_sampler.height * tex_sampler.color_channels;
        vmaCopyMemoryToAllocation(backend->allocator, tex_sampler.data, staging_buf.allocation, staging_buf_offset, byte_size);
        staging_buf_offset += byte_size;
    }

    DescriptorWriter descriptor_writer;
    uint32_t         texture_buf_offset = 0;
    for (const auto& tex_sampler : tex_samplers) {

        VkExtent2D extent = {
            .width  = tex_sampler.width,
            .height = tex_sampler.height,
        };

        AllocatedImage tex_image = allocated_image_create(backend->device_ctx.logical_device, backend->allocator,
                                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_VIEW_TYPE_2D, extent,
                                                          VK_FORMAT_R8G8B8A8_UNORM);

        VkBufferImageCopy copy_region               = {};
        copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel       = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount     = 1;
        copy_region.imageExtent.width               = tex_sampler.width;
        copy_region.imageExtent.height              = tex_sampler.height;
        copy_region.imageExtent.depth               = 1;
        copy_region.bufferOffset                    = texture_buf_offset;

        backend_immediate_submit(backend, [&](VkCommandBuffer cmd) {
            vk_image_memory_barrier_insert(cmd, tex_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

            vkCmdCopyBufferToImage(cmd, staging_buf.buffer, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            vk_image_memory_barrier_insert(cmd, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
        });

        const uint32_t byte_size = tex_sampler.width * tex_sampler.height * tex_sampler.color_channels;
        texture_buf_offset += byte_size;

        desc_writer_write_image_desc(&descriptor_writer, backend->texture_desc_binding, tex_image.image_view, tex_sampler.sampler,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, backend->tex_images.size());
        desc_writer_update_desc_set(&descriptor_writer, backend->device_ctx.logical_device, backend->graphics_desc_set);
        desc_writer_clear(&descriptor_writer);

        backend->tex_images.push_back(tex_image);
    }
    allocated_buffer_destroy(backend->allocator, &staging_buf);

    return descriptor_index_offset;
}

void render_sky_box(const VkBackend* backend, VkCommandBuffer cmd_buf, uint32_t vert_shader_i, uint32_t frag_shader_i) {

    vkCmdSetCullMode(cmd_buf, VK_CULL_MODE_NONE);

    vkCmdSetDepthWriteEnable(cmd_buf, VK_FALSE);

    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.vert_shaders[vert_shader_i].stage,
                                         &backend->shader_ctx.vert_shaders[vert_shader_i].shader);

    backend->ext_ctx.vkCmdBindShadersEXT(cmd_buf, 1, &backend->shader_ctx.frag_shaders[frag_shader_i].stage,
                                         &backend->shader_ctx.frag_shaders[frag_shader_i].shader);

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

    const VkDescriptorSet desc_sets[2] = {backend->scene_desc_sets[get_curr_frame_idx(backend)], backend->graphics_desc_set};
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

    shader_ctx_init(&backend->shader_ctx, &backend->ext_ctx, backend->device_ctx.logical_device);

    swapchain_ctx_deinit(&backend->swapchain_context, backend->device_ctx.logical_device, backend->instance);

    desc_allocator_deinit(&backend->scene_desc_allocator, backend->device_ctx.logical_device);
    desc_allocator_deinit(&backend->graphics_desc_allocator, backend->device_ctx.logical_device);

    for (const auto& tex_image : backend->tex_images) {
        allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &tex_image);
    }

    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_image);
    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->depth_image);
    allocated_image_destroy(backend->device_ctx.logical_device, backend->allocator, &backend->color_resolve_image);

    for (const auto& scene_buf : backend->scene_data_buffers) {
        allocated_buffer_destroy(backend->allocator, &scene_buf);
    }

    allocated_buffer_destroy(backend->allocator, &backend->mat_buffer);

    vkDestroySampler(backend->device_ctx.logical_device, backend->default_nearest_sampler, nullptr);
    vkDestroySampler(backend->device_ctx.logical_device, backend->default_linear_sampler, nullptr);

    vkDestroyPipeline(backend->device_ctx.logical_device, backend->grid_pipeline_info.pipeline, nullptr);

    vkDestroyPipelineLayout(backend->device_ctx.logical_device, backend->grid_pipeline_info.pipeline_layout, nullptr);
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