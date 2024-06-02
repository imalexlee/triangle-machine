#include "global_utils.h"
#include "imgui.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_pipeline.h"
#include "vk_backend/vk_scene.h"
#include "vk_init.h"
#include "vk_options.h"
#include <array>
#include <cassert>
#include <chrono>
#include <core/camera.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fastgltf/types.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <glm/ext/quaternion_transform.hpp>
#include <span>
#include <string>
#include <unistd.h>
#include <vk_backend/vk_device.h>
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

  _device_context.create(_instance, surface);
  _swapchain_context.create(_instance, _device_context, surface, VK_PRESENT_MODE_FIFO_KHR);

  create_allocator();
  create_desc_layouts();

  for (Frame& frame : _frames) {
    frame.create(_device_context.logical_device, _allocator,
                 _device_context.queues.graphics_family_index, _global_desc_set_layout);
  }

  _image_extent.width = window.width;
  _image_extent.height = window.height;

  // if msaa is enabled, we will transfer the color resolve image to the swapchain
  // and leave the larger color image as transient
  VkImageUsageFlags color_img_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  if constexpr (vk_opts::msaa_enabled) {
    _color_resolve_image =
        create_image(_device_context.logical_device, _allocator,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    color_img_flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  } else {
    color_img_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  _color_image =
      create_image(_device_context.logical_device, _allocator, color_img_flags, _image_extent,
                   VK_FORMAT_R16G16B16A16_SFLOAT, _device_context.raster_samples);

  _depth_image = create_image(_device_context.logical_device, _allocator,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, _image_extent,
                              VK_FORMAT_D32_SFLOAT, _device_context.raster_samples);

  // create color attachments and rendering information from our allocated images
  configure_render_resources();

  _imm_fence = create_fence(_device_context.logical_device, VK_FENCE_CREATE_SIGNALED_BIT);
  _imm_cmd_context.create(_device_context.logical_device,
                          _device_context.queues.graphics_family_index,
                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  _camera = &camera;

  create_default_data();
  create_pipelines();
  create_gui(window);

  load_scenes();

  if constexpr (vk_opts::validation_enabled) {
    _debugger.create(_instance, _device_context.logical_device);
    configure_debugger();
  }
}

// adding names to these 64 bit handles helps a lot when reading validation errors
void VkBackend::configure_debugger() {
  _debugger.set_handle_name(_color_image.image, VK_OBJECT_TYPE_IMAGE, "color image");
  _debugger.set_handle_name(_depth_image.image, VK_OBJECT_TYPE_IMAGE, "depth image");

  _debugger.set_handle_name(_color_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "color image view");
  _debugger.set_handle_name(_depth_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW, "depth image view");

  if constexpr (vk_opts::msaa_enabled) {
    _debugger.set_handle_name(_color_resolve_image.image, VK_OBJECT_TYPE_IMAGE,
                              "color resolve image");
    _debugger.set_handle_name(_color_resolve_image.image_view, VK_OBJECT_TYPE_IMAGE_VIEW,
                              "color resolve image view");
  }

  for (size_t i = 0; i < _frames.size(); i++) {
    Frame& frame = _frames[i];
    _debugger.set_handle_name(frame.command_context.primary_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER,
                              "frame " + std::to_string(i) + " cmd buf");
  }

  for (size_t i = 0; i < _swapchain_context.images.size(); i++) {
    _debugger.set_handle_name(_swapchain_context.images[i], VK_OBJECT_TYPE_IMAGE,
                              "swapchain image " + std::to_string(i));
    _debugger.set_handle_name(_swapchain_context.image_views[i], VK_OBJECT_TYPE_IMAGE_VIEW,
                              "swapchain image view" + std::to_string(i));
  }
}

void VkBackend::create_desc_layouts() {

  DescriptorLayoutBuilder layout_builder;
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  _global_desc_set_layout = layout_builder.build(
      _device_context.logical_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  layout_builder.clear();
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  _mat_desc_set_layout = layout_builder.build(
      _device_context.logical_device, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

  layout_builder.clear();
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  _draw_obj_desc_set_layout = layout_builder.build(
      _device_context.logical_device, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
}

void VkBackend::configure_render_resources() {
  _scene_clear_value = {.color = {{1.f, 1.f, 1.f, 1.f}}};

  VkImageView resolve_img_view = nullptr;
  VkAttachmentStoreOp color_store_ap = VK_ATTACHMENT_STORE_OP_STORE;

  if constexpr (vk_opts::msaa_enabled) {
    resolve_img_view = _color_resolve_image.image_view;

    // dont care about the multisampled buffer if it'll resolve into another img anyways
    color_store_ap = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }

  _scene_color_attachment =
      create_color_attachment_info(_color_image.image_view, &_scene_clear_value,
                                   VK_ATTACHMENT_LOAD_OP_CLEAR, color_store_ap, resolve_img_view);

  _scene_depth_attachment = create_depth_attachment_info(
      _depth_image.image_view, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  _scene_rendering_info =
      create_rendering_info(_scene_color_attachment, _scene_depth_attachment, _image_extent);
}

void VkBackend::load_scenes() { _scene = load_scene(this, "../../assets/glb/crypt_location.glb"); }

void VkBackend::create_default_data() {

  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_LINEAR;
  sampler_ci.minFilter = VK_FILTER_LINEAR;
  VK_CHECK(vkCreateSampler(_device_context.logical_device, &sampler_ci, nullptr,
                           &_default_linear_sampler));

  sampler_ci.magFilter = VK_FILTER_NEAREST;
  sampler_ci.minFilter = VK_FILTER_NEAREST;
  VK_CHECK(vkCreateSampler(_device_context.logical_device, &sampler_ci, nullptr,
                           &_default_nearest_sampler));

  _default_texture = upload_texture_image((void*)white_image.data(), VK_IMAGE_USAGE_SAMPLED_BIT,
                                          CHECKER_WIDTH, CHECKER_WIDTH);
}

void VkBackend::create_gui(Window& window) {

  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

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

  VK_CHECK(vkCreateDescriptorPool(_device_context.logical_device, &pool_ci, nullptr,
                                  &_imm_descriptor_pool));

  VkPipelineRenderingCreateInfoKHR pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipeline_info.pColorAttachmentFormats = &_color_image.image_format;
  pipeline_info.colorAttachmentCount = 1;
  pipeline_info.depthAttachmentFormat = _depth_image.image_format;

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _device_context.physical_device;
  init_info.Device = _device_context.logical_device;
  init_info.Queue = _device_context.queues.graphics;
  init_info.DescriptorPool = _imm_descriptor_pool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo = pipeline_info;
  init_info.MSAASamples = (VkSampleCountFlagBits)_device_context.raster_samples;

  ImGui_ImplVulkan_Init(&init_info);
}

auto time1 = std::chrono::high_resolution_clock::now();
void VkBackend::update_scene() {
  auto start_time = system_clock::now();
  _camera->update();

  glm::mat4 model = glm::mat4{1.f};
  //* glm::rotate(glm::mat4{1.f}, glm::radians(time_span.count() * 30), glm::vec3{0, 1, 0});

  glm::mat4 projection = glm::perspective(glm::radians(60.f),
                                          (float)_swapchain_context.extent.width /
                                              (float)_swapchain_context.extent.height,
                                          10000.0f, 0.1f);

  projection[1][1] *= -1;

  _scene_data.view_proj = projection * _camera->view * model;
  _scene_data.eye_pos = _camera->position;

  auto end_time = system_clock::now();
  auto dur = duration<float>(end_time - start_time);
  if (_frame_num % 60 == 0) {
    _stats.scene_update_time = duration_cast<nanoseconds>(dur).count() / 1000.f;
  }
}

void VkBackend::update_global_descriptors() {
  get_current_frame().update_global_desc_set(_device_context.logical_device, _allocator,
                                             _scene_data);
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

  ImGui::Text("Host buffer recording: %.3f us", _stats.draw_time);
  ImGui::Text("Frame time: %.3f ms (%.1f FPS)", _stats.frame_time, 1000.f / _stats.frame_time);
  ImGui::Text("Scene update time: %.3f us", _stats.scene_update_time);

  ImGui::End();

  ImGui::Render();
}

void VkBackend::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &_imm_fence));

  _imm_cmd_context.begin_primary_buffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  function(_imm_cmd_context.primary_buffer);

  _imm_cmd_context.submit_primary_buffer(_device_context.queues.graphics, nullptr, nullptr,
                                         _imm_fence);

  VK_CHECK(vkWaitForFences(_device_context.logical_device, 1, &_imm_fence, VK_TRUE,
                           vk_opts::timeout_dur));
}

void VkBackend::create_allocator() {
  VmaAllocatorCreateInfo allocator_info{};
  allocator_info.device = _device_context.logical_device;
  allocator_info.physicalDevice = _device_context.physical_device;
  allocator_info.instance = _instance;
  allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&allocator_info, &_allocator));
}

void VkBackend::create_instance() {

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "awesome app";
  app_info.pEngineName = "awesome engine";
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
  // cosider passing the shader locations in from the engine instead of here
  VkShaderModule vert_shader = load_shader_module(
      _device_context.logical_device, "../../shaders/vertex/indexed_triangle.vert.glsl.spv");
  VkShaderModule frag_shader = load_shader_module(
      _device_context.logical_device, "../../shaders/fragment/simple_lighting.frag.glsl.spv");

  builder.set_shader_stages(vert_shader, frag_shader);
  builder.disable_blending();
  builder.set_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_raster_culling(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  builder.set_raster_poly_mode(VK_POLYGON_MODE_FILL);
  builder.set_multisample_state((VkSampleCountFlagBits)_device_context.raster_samples);

  builder.set_depth_stencil_state(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_render_info(_color_image.image_format, _depth_image.image_format);

  // all frames have the same layout so you can use the first one's layout
  std::array<VkDescriptorSetLayout, 3> set_layouts{_global_desc_set_layout, _mat_desc_set_layout,
                                                   _draw_obj_desc_set_layout};

  builder.set_layout(set_layouts, {}, 0);

  _opaque_pipeline_info = builder.build_pipeline(_device_context.logical_device);

  builder.enable_blending_alphablend();
  builder.set_depth_stencil_state(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

  _transparent_pipeline_info = builder.build_pipeline(_device_context.logical_device);

  _deletion_queue.push_persistant([=, this]() {
    vkDestroyShaderModule(_device_context.logical_device, vert_shader, nullptr);
    vkDestroyShaderModule(_device_context.logical_device, frag_shader, nullptr);
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

  // wait for previous command buffer to finish executing
  vkWaitForFences(_device_context.logical_device, 1, &current_frame.render_fence, VK_TRUE,
                  vk_opts::timeout_dur);

  uint32_t swapchain_image_index;
  VkResult result = vkAcquireNextImageKHR(
      _device_context.logical_device, _swapchain_context.swapchain, vk_opts::timeout_dur,
      current_frame.present_semaphore, nullptr, &swapchain_image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // probably window resized. glfw event will trigger resize in that case, so just abort
    return;
  }

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &current_frame.render_fence));

  VkImage swapchain_image = _swapchain_context.images[swapchain_image_index];

  VkCommandBufferBeginInfo command_buffer_bi{};
  command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_bi.pNext = nullptr;
  command_buffer_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(current_frame.command_context.primary_buffer, &command_buffer_bi));

  insert_image_memory_barrier(cmd_buffer, _color_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkImage copy_img;
  if constexpr (vk_opts::msaa_enabled) {
    copy_img = _color_resolve_image.image;
    insert_image_memory_barrier(cmd_buffer, _color_resolve_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  } else {
    copy_img = _color_image.image;
  }

  insert_image_memory_barrier(cmd_buffer, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  vkCmdBeginRendering(cmd_buffer, &_scene_rendering_info);

  render_geometry(cmd_buffer);

  render_ui(cmd_buffer);

  vkCmdEndRendering(cmd_buffer);

  insert_image_memory_barrier(cmd_buffer, copy_img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // copy rendered image onto swapchain image
  blit_image(cmd_buffer, copy_img, swapchain_image, _swapchain_context.extent, _image_extent);

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // tweak stage masks to make it more optimal
  VkSemaphoreSubmitInfo wait_semaphore_si = create_semaphore_submit_info(
      current_frame.present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkSemaphoreSubmitInfo signal_semaphore_si = create_semaphore_submit_info(
      current_frame.render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

  current_frame.command_context.submit_primary_buffer(_device_context.queues.graphics,
                                                      &wait_semaphore_si, &signal_semaphore_si,
                                                      current_frame.render_fence);

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.pSwapchains = &_swapchain_context.swapchain;
  present_info.swapchainCount = 1;
  present_info.pImageIndices = &swapchain_image_index;
  present_info.pWaitSemaphores = &current_frame.render_semaphore;
  present_info.waitSemaphoreCount = 1;

  result = vkQueuePresentKHR(_device_context.queues.present, &present_info);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // probably window resized. glfw event will trigger resize in that case, so just abort
    return;
  }

  _frame_num++;

  if (_frame_num % 60 == 0) {
    auto end_time = system_clock::now();
    auto dur = duration<float>(end_time - start_frame_time);
    _stats.frame_time = duration_cast<microseconds>(dur).count() / 1000.f;
  }
}

/*  Resize callback needs to conform to the interface for the glfw callback.
 *  our resize method doesn't need these paramaters
 */
void VkBackend::resize_callback([[maybe_unused]] int new_width, [[maybe_unused]] int new_height) {
  active_backend->resize();
}

void VkBackend::resize() {
  vkDeviceWaitIdle(_device_context.logical_device);

  /* swapchain gets current width and height by querying the system capabilities
   * which means we don't explicitly need to pass in those params
   */
  _swapchain_context.reset_swapchain(_device_context);

  destroy_image(_device_context.logical_device, _allocator, _depth_image);
  destroy_image(_device_context.logical_device, _allocator, _color_image);

  _image_extent.width = _swapchain_context.extent.width;
  _image_extent.height = _swapchain_context.extent.height;

  // if msaa is enabled, we will transfer the color resolve image to the swapchain
  // and leave the larger color image as transient
  VkImageUsageFlags color_img_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  if constexpr (vk_opts::msaa_enabled) {
    destroy_image(_device_context.logical_device, _allocator, _color_resolve_image);

    _color_resolve_image =
        create_image(_device_context.logical_device, _allocator,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                     _image_extent, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    color_img_flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  } else {
    color_img_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  _color_image =
      create_image(_device_context.logical_device, _allocator, color_img_flags, _image_extent,
                   VK_FORMAT_R16G16B16A16_SFLOAT, _device_context.raster_samples);

  _depth_image = create_image(_device_context.logical_device, _allocator,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                              _image_extent, VK_FORMAT_D32_SFLOAT, _device_context.raster_samples);

  configure_render_resources();

  for (Frame& frame : _frames) {
    frame.reset_sync_structures(_device_context.logical_device);
  }
}

void VkBackend::render_geometry(VkCommandBuffer cmd_buf) {
  auto buffer_recording_start = system_clock::now();

  update_global_descriptors();

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
                              current_pipeline_info.pipeline_layout, 1, 1, &obj.mat_desc_set, 0,
                              nullptr);
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
                          &get_current_frame().global_desc_set, 0, nullptr);

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

MeshBuffers VkBackend::upload_mesh_buffers(std::span<uint32_t> indices,
                                           std::span<Vertex> vertices) {
  const size_t vertex_buffer_bytes = vertices.size() * sizeof(Vertex);
  const size_t index_buffer_bytes = indices.size() * sizeof(uint32_t);

  AllocatedBuffer staging_buf = create_buffer(
      vertex_buffer_bytes + index_buffer_bytes, _allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  vmaCopyMemoryToAllocation(_allocator, vertices.data(), staging_buf.allocation, 0,
                            vertex_buffer_bytes);

  vmaCopyMemoryToAllocation(_allocator, indices.data(), staging_buf.allocation, vertex_buffer_bytes,
                            index_buffer_bytes);

  MeshBuffers new_mesh_buffer;
  new_mesh_buffer.vertices =
      create_buffer(vertex_buffer_bytes, _allocator,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY, 0);
  new_mesh_buffer.indices =
      create_buffer(index_buffer_bytes, _allocator,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY, 0);

  immediate_submit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_buffer_region{};
    vertex_buffer_region.size = vertex_buffer_bytes;
    vertex_buffer_region.srcOffset = 0;
    vertex_buffer_region.dstOffset = 0;

    vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.vertices.buffer, 1,
                    &vertex_buffer_region);

    VkBufferCopy index_buffer_region{};
    index_buffer_region.size = index_buffer_bytes;
    index_buffer_region.srcOffset = vertex_buffer_bytes;
    index_buffer_region.dstOffset = 0;

    vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.indices.buffer, 1,
                    &index_buffer_region);
  });

  destroy_buffer(_allocator, staging_buf);
  return new_mesh_buffer;
}

AllocatedImage VkBackend::upload_texture_image(void* data, VkImageUsageFlags usage, uint32_t height,
                                               uint32_t width) {
  VkExtent2D extent{.width = width, .height = height};

  uint32_t data_size = width * height * sizeof(uint32_t);

  AllocatedBuffer staging_buf = create_buffer(
      data_size, _allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  vmaCopyMemoryToAllocation(_allocator, data, staging_buf.allocation, 0, data_size);

  AllocatedImage new_texture;
  new_texture =
      create_image(_device_context.logical_device, _allocator,
                   usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent, VK_FORMAT_R8G8B8A8_UNORM);

  VkBufferImageCopy copy_region;
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;

  copy_region.imageOffset = {.x = 0, .y = 0, .z = 0};
  copy_region.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1};

  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;

  immediate_submit([&](VkCommandBuffer cmd) {
    insert_image_memory_barrier(cmd, new_texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    insert_image_memory_barrier(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });

  destroy_buffer(_allocator, staging_buf);

  return new_texture;
}

void VkBackend::destroy() {

  vkDeviceWaitIdle(_device_context.logical_device);
  DEBUG_PRINT("destroying Vulkan Backend");

  fmt::println("average draw time: {}", _stats.total_draw_time / _frame_num);

  _deletion_queue.flush();

  if constexpr (vk_opts::validation_enabled) {
    _debugger.destroy();
  }

  for (Frame& frame : _frames) {
    frame.destroy();
    destroy_buffer(_allocator, frame.scene_data_buffer);
  }

  destroy_scene(this, _scene);

  ImGui_ImplGlfw_Shutdown();
  ImGui_ImplVulkan_Shutdown();

  ImGui::DestroyContext();

  vkDestroyDescriptorPool(_device_context.logical_device, _imm_descriptor_pool, nullptr);

  destroy_image(_device_context.logical_device, _allocator, _color_image);
  destroy_image(_device_context.logical_device, _allocator, _depth_image);
  destroy_image(_device_context.logical_device, _allocator, _default_texture);
  if constexpr (vk_opts::msaa_enabled) {
    destroy_image(_device_context.logical_device, _allocator, _color_resolve_image);
  }

  vkDestroySampler(_device_context.logical_device, _default_nearest_sampler, nullptr);
  vkDestroySampler(_device_context.logical_device, _default_linear_sampler, nullptr);

  vkDestroyPipelineLayout(_device_context.logical_device, _opaque_pipeline_info.pipeline_layout,
                          nullptr);
  vkDestroyPipelineLayout(_device_context.logical_device,
                          _transparent_pipeline_info.pipeline_layout, nullptr);

  vkDestroyDescriptorSetLayout(_device_context.logical_device, _global_desc_set_layout, nullptr);
  vkDestroyDescriptorSetLayout(_device_context.logical_device, _mat_desc_set_layout, nullptr);
  vkDestroyDescriptorSetLayout(_device_context.logical_device, _draw_obj_desc_set_layout, nullptr);

  vkDestroyPipeline(_device_context.logical_device, _opaque_pipeline_info.pipeline, nullptr);
  vkDestroyPipeline(_device_context.logical_device, _transparent_pipeline_info.pipeline, nullptr);

  vkDestroyFence(_device_context.logical_device, _imm_fence, nullptr);

  vmaDestroyAllocator(_allocator);

  _imm_cmd_context.destroy();
  _swapchain_context.destroy();
  _device_context.destroy();

  vkDestroyInstance(_instance, nullptr);
}
