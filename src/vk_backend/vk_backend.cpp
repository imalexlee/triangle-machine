#include "global_utils.h"
#include "imgui.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_pipeline.h"
#include "vk_backend/vk_scene.h"
#include "vk_init.h"
#include <cassert>
#include <chrono>
#include <core/camera.h>
#include <cstring>
#include <fastgltf/types.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/ext/quaternion_transform.hpp>
#include <span>
#include <string>
#include <vk_backend/vk_command.h>
#include <vk_backend/vk_device.h>
#include <vk_backend/vk_frame.h>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>
#include <vk_backend/vk_utils.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "vk_backend/resources/vk_loader.h"
#include "vk_backend/vk_backend.h"
#include "vk_backend/vk_sync.h"

using namespace std::chrono;

static VkBackend* active_backend = nullptr;

void VkBackend::create(Window& window, Camera& camera) {
    assert(active_backend == nullptr);
    active_backend = this;

    create_instance();

    VkSurfaceKHR surface = window.get_vulkan_surface(_instance);

    init_device_context(&device_ctx, _instance, surface);
    init_swapchain_context(&_swapchain_context, &device_ctx, surface);

    create_allocator();
    create_desc_layouts();

    for (Frame& frame : _frames) {
        init_frame(&frame, device_ctx.logical_device, _allocator,
                   device_ctx.queues.graphics_family_index, _global_desc_set_layout);
    }

    _image_extent.width = window.width;
    _image_extent.height = window.height;

    _color_resolve_image =
        create_image(device_ctx.logical_device, _allocator,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    _color_image =
        create_image(device_ctx.logical_device, _allocator,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, device_ctx.raster_samples);

    _depth_image = create_image(device_ctx.logical_device, _allocator,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, _image_extent,
                                VK_FORMAT_D32_SFLOAT, device_ctx.raster_samples);

    // create color attachments and  rendering information from our allocated images
    configure_render_resources();

    _imm_fence = create_fence(device_ctx.logical_device, VK_FENCE_CREATE_SIGNALED_BIT);
    // _imm_cmd_context.create(device_ctx.logical_device,
    //                         device_ctx.queues.graphics_family_index,
    //                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    init_cmd_context(&imm_cmd_context, device_ctx.logical_device,
                     device_ctx.queues.graphics_family_index,
                     VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    _camera = &camera;

    create_default_data();
    create_pipelines();
    create_gui(window);

    load_scenes();

    if constexpr (vk_opts::validation_enabled) {
        _debugger.create(_instance, device_ctx.logical_device);
        configure_debugger();
    }
}

// adding names to these 64 bit handles helps a lot when reading validation errors
void VkBackend::configure_debugger() {
    _debugger.set_handle_name(_color_image.image, VK_OBJECT_TYPE_IMAGE, "color image");
    _debugger.set_handle_name(_depth_image.image, VK_OBJECT_TYPE_IMAGE, "depth image");

    _debugger.set_handle_name(_color_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                              "color image view");
    _debugger.set_handle_name(_depth_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                              "depth image view");

    _debugger.set_handle_name(_color_resolve_image.image, VK_OBJECT_TYPE_IMAGE,
                              "color resolve image");
    _debugger.set_handle_name(_color_resolve_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                              "color resolve image view");

    for (size_t i = 0; i < _frames.size(); i++) {
        Frame& frame = _frames[i];
        _debugger.set_handle_name(frame.command_context.primary_buffer,
                                  VK_OBJECT_TYPE_COMMAND_BUFFER,
                                  "frame " + std::to_string(i) + " cmd buf");
    }

    for (size_t i = 0; i < _swapchain_context.images.size(); i++) {
        _debugger.set_handle_name(_swapchain_context.images[i], VK_OBJECT_TYPE_IMAGE,
                                  "swapchain  image " + std::to_string(i));
        _debugger.set_handle_name(_swapchain_context.image_views[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                                  "swapchain image view " + std::to_string(i));
    }
}

void VkBackend::create_desc_layouts() {

    DescriptorLayoutBuilder layout_builder;
    add_layout_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    _global_desc_set_layout =
        build_set_layout(&layout_builder, device_ctx.logical_device,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    clear_layout_bindings(&layout_builder);
    add_layout_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    add_layout_binding(&layout_builder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    add_layout_binding(&layout_builder, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    _mat_desc_set_layout =
        build_set_layout(&layout_builder, device_ctx.logical_device,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

    clear_layout_bindings(&layout_builder);
    add_layout_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    _draw_obj_desc_set_layout =
        build_set_layout(&layout_builder, device_ctx.logical_device,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
}

void VkBackend::configure_render_resources() {

    _scene_clear_value = {.color = {{1.f, 1.f, 1.f, 1.f}}};

    _scene_color_attachment = create_color_attachment_info(
        _color_image.image_view, &_scene_clear_value, VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE, _color_resolve_image.image_view);

    _scene_depth_attachment = create_depth_attachment_info(
        _depth_image.image_view, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

    _scene_rendering_info =
        create_rendering_info(_scene_color_attachment, _scene_depth_attachment, _image_extent);
}

void VkBackend::load_scenes() { _scene = load_scene(*this, "../../assets/glb/structure.glb"); }

void VkBackend::create_default_data() {

    _stats.total_fps = 0;
    _stats.total_frame_time = 0;
    _stats.total_draw_time = 0;

    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(
        vkCreateSampler(device_ctx.logical_device, &sampler_ci, nullptr, &_default_linear_sampler));

    sampler_ci.magFilter = VK_FILTER_NEAREST;
    sampler_ci.minFilter = VK_FILTER_NEAREST;
    VK_CHECK(vkCreateSampler(device_ctx.logical_device, &sampler_ci, nullptr,
                             &_default_nearest_sampler));

    _default_texture = upload_texture(*this, (void*)white_image.data(), VK_IMAGE_USAGE_SAMPLED_BIT,
                                      IMAGE_WIDTH, IMAGE_WIDTH);
}

void VkBackend::create_gui(Window& window) {

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForVulkan(window.glfw_window, true);

    ImGui::StyleColorsDark();

    std::array<VkDescriptorPoolSize, 1> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
    };

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.maxSets = 1;
    pool_ci.pPoolSizes = pool_sizes.data();
    pool_ci.poolSizeCount = pool_sizes.size();
    pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK(vkCreateDescriptorPool(device_ctx.logical_device, &pool_ci, nullptr,
                                    &_imm_descriptor_pool));

    VkPipelineRenderingCreateInfoKHR pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_info.pColorAttachmentFormats = &_color_image.image_format;
    pipeline_info.colorAttachmentCount = 1;
    pipeline_info.depthAttachmentFormat = _depth_image.image_format;

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = device_ctx.physical_device;
    init_info.Device = device_ctx.logical_device;
    init_info.Queue = device_ctx.queues.graphics;
    init_info.DescriptorPool = _imm_descriptor_pool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = pipeline_info;
    init_info.MSAASamples = (VkSampleCountFlagBits)device_ctx.raster_samples;

    ImGui_ImplVulkan_Init(&init_info);
}

auto time1 = std::chrono::high_resolution_clock::now();
void VkBackend::update_scene() {
    auto start_time = system_clock::now();
    _camera->update();

    glm::mat4 model = glm::mat4{1.f};
    //* glm::rotate(glm::mat4{1.f},
    // glm::radians(time_span.count()
    // * 30),
    // glm::vec3{0, 1,
    // 0});

    glm::mat4 projection = glm::perspective(glm::radians(60.f),
                                            (float)_swapchain_context.extent.width /
                                                (float)_swapchain_context.extent.height,
                                            10000.0f, 0.1f);

    projection[1][1] *= -1;

    _frame_data.view_proj = projection * _camera->view * model;
    _frame_data.eye_pos = _camera->position;

    auto end_time = system_clock::now();
    auto dur = duration<float>(end_time - start_time);
    if (_frame_num % 60 == 0) {
        _stats.scene_update_time = duration_cast<nanoseconds>(dur).count() / 1000.f;
    }
}

bool show_demo_window = true;
bool show_another_window = false;
void VkBackend::update_ui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    static bool show_window = true;

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("Stats", &show_window, window_flags);

    ImGui::Text("Host buffer "
                "recording: "
                "%.3f us",
                _stats.draw_time);
    ImGui::Text("Frame time: "
                "%.3f ms "
                "(%.1f FPS)",
                _stats.frame_time, 1000.f / _stats.frame_time);
    ImGui::Text("Scene "
                "update "
                "time: %.3f "
                "us",
                _stats.scene_update_time);

    ImGui::End();

    ImGui::Render();
}

void VkBackend::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(device_ctx.logical_device, 1, &_imm_fence));

    begin_primary_buffer(&imm_cmd_context, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(imm_cmd_context.primary_buffer);

    submit_primary_buffer(&imm_cmd_context, device_ctx.queues.graphics, nullptr, nullptr,
                          _imm_fence);

    VK_CHECK(
        vkWaitForFences(device_ctx.logical_device, 1, &_imm_fence, VK_TRUE, vk_opts::timeout_dur));
}

void VkBackend::create_allocator() {
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.device = device_ctx.logical_device;
    allocator_info.physicalDevice = device_ctx.physical_device;
    allocator_info.instance = _instance;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocator_info, &_allocator));
}

void VkBackend::create_instance() {

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "awesome app";
    app_info.pEngineName = "awesome "
                           "engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> instance_extensions = get_instance_extensions();

    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    instance_ci.flags = 0;
    instance_ci.ppEnabledExtensionNames = instance_extensions.data();
    instance_ci.enabledExtensionCount = instance_extensions.size();

    VkDebugUtilsMessengerCreateInfoEXT debug_ci;
    VkValidationFeaturesEXT validation_features;
    std::array<const char*, 1> validation_layers;

    if constexpr (vk_opts::validation_enabled) {
        debug_ci = _debugger.create_messenger_info();
        validation_features = _debugger.create_validation_features();
        validation_layers = _debugger.create_validation_layers();

        validation_features.pNext = &debug_ci;
        instance_ci.pNext = &validation_features;
        instance_ci.enabledLayerCount = validation_layers.size();
        instance_ci.ppEnabledLayerNames = validation_layers.data();
    }

    VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &_instance));
}

void VkBackend::create_pipelines() {
    PipelineBuilder builder;
    VkShaderModule vert_shader = load_shader_module(
        device_ctx.logical_device, "../../shaders/vertex/indexed_triangle.vert.glsl.spv");
    VkShaderModule frag_shader = load_shader_module(
        device_ctx.logical_device, "../../shaders/fragment/simple_lighting.frag.glsl.spv");

    builder.set_shader_stages(vert_shader, frag_shader);
    builder.disable_blending();
    builder.set_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.set_raster_culling(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    builder.set_raster_poly_mode(VK_POLYGON_MODE_FILL);
    builder.set_multisample_state((VkSampleCountFlagBits)device_ctx.raster_samples);

    builder.set_depth_stencil_state(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    builder.set_render_info(_color_image.image_format, _depth_image.image_format);

    std::array<VkDescriptorSetLayout, 3> set_layouts{_global_desc_set_layout, _mat_desc_set_layout,
                                                     _draw_obj_desc_set_layout};

    builder.set_layout(set_layouts, {}, 0);

    _opaque_pipeline_info = builder.build_pipeline(device_ctx.logical_device);

    builder.enable_blending_alphablend();
    builder.set_depth_stencil_state(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    _transparent_pipeline_info = builder.build_pipeline(device_ctx.logical_device);

    _deletion_queue.push_persistant([=, this]() {
        vkDestroyShaderModule(device_ctx.logical_device, vert_shader, nullptr);
        vkDestroyShaderModule(device_ctx.logical_device, frag_shader, nullptr);
    });
}

std::vector<const char*> VkBackend::get_instance_extensions() {
    uint32_t count{0};
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> extensions;
    for (size_t i = 0; i < count; i++) {
        extensions.emplace_back(glfw_extensions[i]);
    }
    if constexpr (vk_opts::validation_enabled) {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

void VkBackend::draw() {
    auto start_frame_time = system_clock::now();

    update_scene();
    update_ui();

    Frame& current_frame = get_current_frame();
    VkCommandBuffer cmd_buffer = current_frame.command_context.primary_buffer;

    vkWaitForFences(device_ctx.logical_device, 1, &current_frame.render_fence, VK_TRUE,
                    vk_opts::timeout_dur);

    set_frame_data(&current_frame, device_ctx.logical_device, _allocator, &_frame_data);

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(device_ctx.logical_device, _swapchain_context.swapchain,
                                            vk_opts::timeout_dur, current_frame.present_semaphore,
                                            nullptr, &swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize();
        return;
    }

    VK_CHECK(vkResetFences(device_ctx.logical_device, 1, &current_frame.render_fence));

    VkImage swapchain_image = _swapchain_context.images[swapchain_image_index];

    begin_primary_buffer(&current_frame.command_context,
                         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    insert_image_memory_barrier(cmd_buffer, _color_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    insert_image_memory_barrier(cmd_buffer, _color_resolve_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    insert_image_memory_barrier(cmd_buffer, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd_buffer, &_scene_rendering_info);

    render_geometry(cmd_buffer);

    render_ui(cmd_buffer);

    vkCmdEndRendering(cmd_buffer);

    insert_image_memory_barrier(cmd_buffer, _color_resolve_image.image,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    blit_image(cmd_buffer, _color_resolve_image.image, swapchain_image, _swapchain_context.extent,
               _image_extent);

    insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkSemaphoreSubmitInfo wait_semaphore_si = create_semaphore_submit_info(
        current_frame.present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSemaphoreSubmitInfo signal_semaphore_si = create_semaphore_submit_info(
        current_frame.render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

    submit_primary_buffer(&current_frame.command_context, device_ctx.queues.graphics,
                          &wait_semaphore_si, &signal_semaphore_si, current_frame.render_fence);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.pSwapchains = &_swapchain_context.swapchain;
    present_info.swapchainCount = 1;
    present_info.pImageIndices = &swapchain_image_index;
    present_info.pWaitSemaphores = &current_frame.render_semaphore;
    present_info.waitSemaphoreCount = 1;

    result = vkQueuePresentKHR(device_ctx.queues.present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        resize();
        return;
    }

    _frame_num++;

    auto end_time = system_clock::now();
    auto dur = duration<float>(end_time - start_frame_time);
    _stats.total_fps += 1000000.f / (float)duration_cast<microseconds>(dur).count();
    _stats.total_frame_time += duration_cast<microseconds>(dur).count();
    if (_frame_num % 60 == 0) {
        _stats.frame_time = duration_cast<microseconds>(dur).count() / 1000.f;
    }
}

// currently unused
void VkBackend::resize_callback([[maybe_unused]] int new_width, [[maybe_unused]] int new_height) {
    active_backend->resize();
}

void VkBackend::resize() {
    vkDeviceWaitIdle(device_ctx.logical_device);

    reset_swapchain_context(&_swapchain_context, &device_ctx);

    destroy_image(device_ctx.logical_device, _allocator, _depth_image);
    destroy_image(device_ctx.logical_device, _allocator, _color_image);

    _image_extent.width = _swapchain_context.extent.width;
    _image_extent.height = _swapchain_context.extent.height;

    destroy_image(device_ctx.logical_device, _allocator, _color_resolve_image);

    _color_resolve_image =
        create_image(device_ctx.logical_device, _allocator,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    _color_image =
        create_image(device_ctx.logical_device, _allocator,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, device_ctx.raster_samples);

    _depth_image = create_image(device_ctx.logical_device, _allocator,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, _image_extent,
                                VK_FORMAT_D32_SFLOAT, device_ctx.raster_samples);

    configure_render_resources();

    for (Frame& frame : _frames) {
        reset_frame_sync(&frame, device_ctx.logical_device);
    }
}

void VkBackend::render_geometry(VkCommandBuffer cmd_buf) {
    auto buffer_recording_start = system_clock::now();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = _image_extent.width;
    viewport.height = _image_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = _image_extent;
    scissor.offset = {0, 0};

    PipelineInfo current_pipeline_info;
    VkDescriptorSet current_mat_desc = VK_NULL_HANDLE;

    const auto record_obj = [&](const DrawObject& obj) {
        if (obj.mat_desc_set != current_mat_desc) {
            current_mat_desc = obj.mat_desc_set;
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    current_pipeline_info.pipeline_layout, 1, 1, &obj.mat_desc_set,
                                    0, nullptr);
        }

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                current_pipeline_info.pipeline_layout, 2, 1, &obj.obj_desc_set, 0,
                                nullptr);

        vkCmdBindIndexBuffer(cmd_buf, obj.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd_buf, obj.indices_count, 1, obj.indices_start, 0, 0);
    };

    current_pipeline_info = _opaque_pipeline_info;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_info.pipeline);

    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            current_pipeline_info.pipeline_layout, 0, 1,
                            &get_current_frame().desc_set, 0, nullptr);

    for (const DrawObject& obj : _scene.opaque_objs) {
        record_obj(obj);
    }

    current_pipeline_info = _transparent_pipeline_info;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_info.pipeline);

    for (const DrawObject& obj : _scene.transparent_objs) {
        record_obj(obj);
    }

    auto end_time = system_clock::now();
    auto dur = duration<float>(end_time - buffer_recording_start);
    _stats.total_draw_time += (uint32_t)duration_cast<microseconds>(dur).count();
    if (_frame_num % 60 == 0) {
        _stats.draw_time = duration_cast<microseconds>(dur).count();
    }
}

void VkBackend::render_ui(VkCommandBuffer cmd_buf) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
}

void VkBackend::destroy() {

    vkDeviceWaitIdle(device_ctx.logical_device);
    DEBUG_PRINT("destroying "
                "Vulkan "
                "Backend");

    fmt::println("average "
                 "draw time: "
                 "{:.3f} us",
                 (float)_stats.total_draw_time / (float)_frame_num);
    fmt::println("average "
                 "frame time: "
                 "{:.3f} ms",
                 (float)_stats.total_frame_time / 1000.f / (float)_frame_num);
    fmt::println("average "
                 "fps: {:.3f}",
                 (float)_stats.total_fps / (float)_frame_num);

    _deletion_queue.flush();

    if constexpr (vk_opts::validation_enabled) {
        _debugger.destroy();
    }

    for (Frame& frame : _frames) {
        deinit_frame(&frame, device_ctx.logical_device);
        destroy_buffer(_allocator, frame.frame_data_buf);
    }

    destroy_scene(*this, _scene);

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();

    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device_ctx.logical_device, _imm_descriptor_pool, nullptr);

    destroy_image(device_ctx.logical_device, _allocator, _color_image);
    destroy_image(device_ctx.logical_device, _allocator, _depth_image);
    destroy_image(device_ctx.logical_device, _allocator, _default_texture);
    destroy_image(device_ctx.logical_device, _allocator, _color_resolve_image);

    vkDestroySampler(device_ctx.logical_device, _default_nearest_sampler, nullptr);
    vkDestroySampler(device_ctx.logical_device, _default_linear_sampler, nullptr);

    vkDestroyPipelineLayout(device_ctx.logical_device, _opaque_pipeline_info.pipeline_layout,
                            nullptr);
    vkDestroyPipelineLayout(device_ctx.logical_device, _transparent_pipeline_info.pipeline_layout,
                            nullptr);

    vkDestroyDescriptorSetLayout(device_ctx.logical_device, _global_desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device_ctx.logical_device, _mat_desc_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device_ctx.logical_device, _draw_obj_desc_set_layout, nullptr);

    vkDestroyPipeline(device_ctx.logical_device, _opaque_pipeline_info.pipeline, nullptr);
    vkDestroyPipeline(device_ctx.logical_device, _transparent_pipeline_info.pipeline, nullptr);

    vkDestroyFence(device_ctx.logical_device, _imm_fence, nullptr);

    vmaDestroyAllocator(_allocator);

    deinit_cmd_context(&imm_cmd_context, device_ctx.logical_device);
    //_swapchain_context.destroy();
    deinit_swapchain_context(&_swapchain_context, device_ctx.logical_device, _instance);
    deinit_device_context(&device_ctx);

    vkDestroyInstance(_instance, nullptr);
}
