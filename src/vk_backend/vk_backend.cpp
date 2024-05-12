#include "vk_backend/vk_pipeline.h"
#include <array>
#include <chrono>
#include <core/camera.h>
#include <cstdint>
#include <cstring>
#include <fastgltf/types.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <future>
#include <glm/ext/matrix_float4x4.hpp>
#include <span>
#include <unistd.h>
#include <vk_backend/vk_utils.h>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_scene.h"
#include "vk_mem_alloc.h"

#include "vk_backend/resources/vk_loader.h"
#include "vk_backend/vk_backend.h"
#include "vk_backend/vk_sync.h"

#ifdef NDEBUG
constexpr bool use_validation_layers = false;
#else
constexpr bool use_validation_layers = true;
#endif // NDEBUG

constexpr uint64_t TIMEOUT_DURATION = 1'000'000'000;
using namespace std::chrono;

void VkBackend::create(Window &window, Camera &camera) {
  create_instance();
  VkSurfaceKHR surface = window.get_vulkan_surface(_instance);

  _device_context.create(_instance, surface);
  _swapchain_context.create(_instance, _device_context, surface, VK_PRESENT_MODE_MAILBOX_KHR);
  create_allocator();

  for (Frame &frame : _frames) {
    fmt::println("creating frame");
    frame.create(_device_context.logical_device, _allocator, _device_context.queues.graphics_family_index);
  }

  VkExtent2D image_extent{
      .width = window.width,
      .height = window.height,
  };

  _draw_image =
      create_image(_device_context.logical_device, _allocator,
                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   image_extent, VK_FORMAT_R16G16B16A16_SFLOAT);

  _depth_image = create_image(_device_context.logical_device, _allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              _swapchain_context.extent, VK_FORMAT_D32_SFLOAT);

  _imm_fence = create_fence(_device_context.logical_device, VK_FENCE_CREATE_SIGNALED_BIT);
  _imm_cmd_context.create(_device_context.logical_device, _device_context.queues.graphics_family_index,
                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  uint32_t thread_count = std::thread::hardware_concurrency();
  _thread_pool.resize(thread_count);

  _camera = &camera;

  create_default_data();
  create_pipelines();
  create_gui(window);

  if (use_validation_layers) {
    _debugger.create(_instance);
  }
}

void VkBackend::create_default_data() {

  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_LINEAR;
  sampler_ci.minFilter = VK_FILTER_LINEAR;
  VK_CHECK(vkCreateSampler(_device_context.logical_device, &sampler_ci, nullptr, &_default_linear_sampler));

  sampler_ci.magFilter = VK_FILTER_NEAREST;
  sampler_ci.minFilter = VK_FILTER_NEAREST;
  VK_CHECK(vkCreateSampler(_device_context.logical_device, &sampler_ci, nullptr, &_default_nearest_sampler));

  _default_texture =
      upload_texture_image((void *)white_image.data(), VK_IMAGE_USAGE_SAMPLED_BIT, CHECKER_WIDTH, CHECKER_WIDTH);

  _scene = load_scene(this, "../../assets/3d/structure.glb");
}

void VkBackend::create_gui(Window &window) {

  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
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

  VK_CHECK(vkCreateDescriptorPool(_device_context.logical_device, &pool_ci, nullptr, &_imm_descriptor_pool));

  VkPipelineRenderingCreateInfoKHR pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipeline_info.pColorAttachmentFormats = &_swapchain_context.format;
  pipeline_info.colorAttachmentCount = 1;
  // pipeline_info.depthAttachmentFormat = _depth_image.image_format;

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
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);
}

void VkBackend::update_scene() {
  auto start_time = system_clock::now();

  _camera->update();

  glm::mat4 model = glm::mat4{1.f};

  glm::mat4 projection = glm::perspective(
      glm::radians(60.f), (float)_swapchain_context.extent.width / (float)_swapchain_context.extent.height, 10000.0f,
      0.1f);

  projection[1][1] *= -1;

  _scene_data.view_proj = projection * _camera->view * model;
  _scene_data.eye_pos = _camera->position;

  auto end_time = system_clock::now();
  auto dur = duration<float>(end_time - start_time);
  _stats.scene_update_time = duration_cast<nanoseconds>(dur).count() / 1000.f;
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

  ImGui::Text("Host buffer recording: %.3f ms", _stats.draw_time);
  ImGui::Text("Frame time: %.3f ms (%.1f FPS)", _stats.frame_time, 1000.f / _stats.frame_time);
  ImGui::Text("Scene update time: %.3f us", _stats.scene_update_time);

  ImGui::End();

  ImGui::Render();
}

void VkBackend::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function) {

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &_imm_fence));

  _imm_cmd_context.begin_primary_buffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  function(_imm_cmd_context.primary_buffer);

  _imm_cmd_context.submit_primary_buffer(_device_context.queues.graphics, nullptr, nullptr, _imm_fence);

  VK_CHECK(vkWaitForFences(_device_context.logical_device, 1, &_imm_fence, VK_TRUE, TIMEOUT_DURATION));
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

  std::vector<const char *> instance_extensions = get_instance_extensions();

  VkInstanceCreateInfo instance_ci{};
  instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_ci.pApplicationInfo = &app_info;
  instance_ci.flags = 0;
  instance_ci.ppEnabledExtensionNames = instance_extensions.data();
  instance_ci.enabledExtensionCount = instance_extensions.size();

  VkDebugUtilsMessengerCreateInfoEXT debug_ci;
  VkValidationFeaturesEXT validation_features;
  std::array<const char *, 1> validation_layers;

  if constexpr (use_validation_layers) {
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
  VkShaderModule vert_shader =
      load_shader_module(_device_context.logical_device, "../../shaders/vertex/indexed_triangle.vert.glsl.spv");
  VkShaderModule frag_shader =
      load_shader_module(_device_context.logical_device, "../../shaders/fragment/simple_lighting.frag.glsl.spv");

  builder.set_shader_stages(vert_shader, frag_shader);
  builder.disable_blending();
  builder.set_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_raster_culling(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  builder.set_raster_poly_mode(VK_POLYGON_MODE_FILL);
  builder.set_multisample_state(VK_SAMPLE_COUNT_1_BIT);
  builder.set_depth_stencil_state(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_render_info(_swapchain_context.format, _depth_image.image_format);

  // VkPushConstantRange push_constant_range{};
  // push_constant_range.size = sizeof(DrawConstants);
  // push_constant_range.offset = 0;
  // push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  // std::array<VkPushConstantRange, 1> push_constant_ranges{push_constant_range};
  // all frames have the same layout so you can use the first one's layout
  std::array<VkDescriptorSetLayout, 3> set_layouts{_frames[0].desc_set_layout, _scene.mat_desc_set_layout,
                                                   _scene.obj_desc_set_layout};

  builder.set_layout(set_layouts, {}, 0);

  PipelineInfo opaque_pipeline_info = builder.build_pipeline(_device_context.logical_device);
  _scene.opaque_pipeline_info = opaque_pipeline_info;

  // for (auto& primitive : _scene.draw_ctx.opaque_primitives) {
  //   primitive.pipeline_info = _scene.draw_ctx.opaque_pipeline_info;
  // }

  builder.enable_blending_alphablend();
  builder.set_depth_stencil_state(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

  PipelineInfo transparent_pipeline_info = builder.build_pipeline(_device_context.logical_device);
  _scene.transparent_pipeline_info = transparent_pipeline_info;

  // for (auto& draw_obj : _scene.draw_objects) {
  //   draw_obj.pipeline_info = _scene.opaque_pipeline_info;
  // }

  // for (auto& new_primitive : _scene.draw_ctx.opaque_primitives) {
  //   _scene.draw_ctx.all_primitives.push_back(new_primitive);
  // }
  //
  // for (auto& new_primitive : _scene.draw_ctx.transparent_primitives) {
  //   _scene.draw_ctx.all_primitives.push_back(new_primitive);
  // }

  _deletion_queue.push_persistant([=, this]() {
    vkDestroyShaderModule(_device_context.logical_device, vert_shader, nullptr);
    vkDestroyShaderModule(_device_context.logical_device, frag_shader, nullptr);
  });
}

std::vector<const char *> VkBackend::get_instance_extensions() {
  uint32_t count{0};
  const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char *> extensions;
  for (size_t i = 0; i < count; i++) {
    extensions.emplace_back(glfw_extensions[i]);
  }
  if constexpr (use_validation_layers) {
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

void VkBackend::draw() {
  auto start_frame_time = system_clock::now();

  update_scene();
  update_ui();

  Frame &current_frame = get_current_frame();
  VkCommandBuffer cmd_buffer = current_frame.command_context.primary_buffer;

  // wait for previous command buffer to finish executing
  vkWaitForFences(_device_context.logical_device, 1, &current_frame.render_fence, VK_TRUE, TIMEOUT_DURATION);

  uint32_t swapchain_image_index;
  VkResult result =
      vkAcquireNextImageKHR(_device_context.logical_device, _swapchain_context.swapchain, TIMEOUT_DURATION,
                            current_frame.present_semaphore, nullptr, &swapchain_image_index);

  auto buffer_recording_start = system_clock::now();

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return;
  }

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &current_frame.render_fence));

  VkCommandBufferBeginInfo command_buffer_bi{};
  command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_bi.pNext = nullptr;
  command_buffer_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  // command_buffer_bi.pInheritanceInfo = &inheritance_info;

  VK_CHECK(vkBeginCommandBuffer(current_frame.command_context.primary_buffer, &command_buffer_bi));

  VkImage swapchain_image = _swapchain_context.images[swapchain_image_index];
  VkImageView swapchain_image_view = _swapchain_context.image_views[swapchain_image_index];

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  insert_image_memory_barrier(cmd_buffer, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  render_geometry(cmd_buffer, _swapchain_context.extent, swapchain_image_view);

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  render_ui(cmd_buffer, _swapchain_context.extent, swapchain_image_view);

  insert_image_memory_barrier(cmd_buffer, swapchain_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // tweak stage masks to make it more optimal
  VkSemaphoreSubmitInfo wait_semaphore_si =
      create_semaphore_submit_info(current_frame.present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkSemaphoreSubmitInfo signal_semaphore_si =
      create_semaphore_submit_info(current_frame.render_semaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

  current_frame.command_context.submit_primary_buffer(_device_context.queues.graphics, &wait_semaphore_si,
                                                      &signal_semaphore_si, current_frame.render_fence);

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
    return;
  }

  _frame_num++;

  auto end_time = system_clock::now();
  auto dur = duration<float>(end_time - buffer_recording_start);
  _stats.draw_time = duration_cast<microseconds>(dur).count() / 1000.f;

  dur = duration<float>(end_time - start_frame_time);
  end_time = system_clock::now();
  _stats.frame_time = duration_cast<microseconds>(dur).count() / 1000.f;
}

void VkBackend::resize() {
  vkDeviceWaitIdle(_device_context.logical_device);
  _swapchain_context.reset_swapchain(_device_context);

  destroy_image(_device_context.logical_device, _allocator, _depth_image);
  _depth_image = create_image(_device_context.logical_device, _allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              _swapchain_context.extent, VK_FORMAT_D32_SFLOAT);

  for (Frame &frame : _frames) {
    frame.reset_sync_structures(_device_context.logical_device);
  }
}

void VkBackend::render_geometry(VkCommandBuffer cmd_buf, VkExtent2D extent, VkImageView image_view) {
  // make this use _draw_image.image after blit image is done
  VkRenderingAttachmentInfo color_attachment =
      create_color_attachment_info(image_view, nullptr, VK_ATTACHMENT_LOAD_OP_CLEAR);

  VkRenderingAttachmentInfo depth_attachment = create_depth_attachment_info(_depth_image.image_view);

  VkRenderingInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{
      .offset = VkOffset2D{0, 0},
      .extent = extent,
  };
  rendering_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.layerCount = 1;
  rendering_info.pStencilAttachment = nullptr;
  rendering_info.pDepthAttachment = &depth_attachment;

  vkCmdBeginRendering(cmd_buf, &rendering_info);

  get_current_frame().clear_scene_desc_set(_device_context.logical_device);
  // allocate an empty set with the layout to hold our global scene data
  VkDescriptorSet scene_desc_set = get_current_frame().create_scene_desc_set(_device_context.logical_device);

  // fill in the buffer with the updated scene data
  SceneData *scene_data = (SceneData *)get_current_frame().scene_data_buffer.allocation->GetMappedData();
  *scene_data = _scene_data;

  // connect buffer that was just filled to a binding then hook it up to the allocated desc set
  DescriptorWriter writer;
  writer.write_buffer(0, get_current_frame().scene_data_buffer.buffer, sizeof(SceneData), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(_device_context.logical_device, scene_desc_set);
  writer.clear();

  // both opaque and transparent have the same layout for now
  VkPipelineLayout current_pipeline_layout = _scene.opaque_pipeline_info.pipeline_layout;

  auto draw = [&, this](const std::vector<DrawObject> &draw_objects, const uint32_t start_idx, const uint32_t stride,
                        uint32_t thread_id) {
    VkCommandBufferInheritanceRenderingInfo inheritance_rendering_info;
    inheritance_rendering_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
    inheritance_rendering_info.pNext = nullptr;
    inheritance_rendering_info.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
    inheritance_rendering_info.colorAttachmentCount = 1;
    inheritance_rendering_info.depthAttachmentFormat = _depth_image.image_format;
    inheritance_rendering_info.pColorAttachmentFormats = &_swapchain_context.format;
    inheritance_rendering_info.viewMask = 0;
    inheritance_rendering_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    inheritance_rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkCommandBufferInheritanceInfo inheritance_info{};
    inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritance_info.pNext = &inheritance_rendering_info;

    VkCommandBufferBeginInfo command_buffer_bi{};
    command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_bi.pNext = nullptr;
    command_buffer_bi.pInheritanceInfo = &inheritance_info;
    command_buffer_bi.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    VkCommandBuffer secondary_buf = get_current_frame().command_context.secondary_buffers[thread_id];

    vkBeginCommandBuffer(secondary_buf, &command_buffer_bi);

    if (start_idx < _scene.trans_start) {
      vkCmdBindPipeline(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _scene.opaque_pipeline_info.pipeline);
    } else {
      vkCmdBindPipeline(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _scene.transparent_pipeline_info.pipeline);
    }
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = extent;
    scissor.offset = {0, 0};

    vkCmdSetViewport(secondary_buf, 0, 1, &viewport);

    vkCmdSetScissor(secondary_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_layout, 0, 1,
                            &scene_desc_set, 0, nullptr);

    VkDescriptorSet material_desc_set = nullptr;

    uint32_t end_pos = std::min(start_idx + stride, (uint32_t)draw_objects.size());

    for (uint32_t i = start_idx; i < end_pos; i++) {

      const auto &draw_obj = draw_objects[i];

      if (i == _scene.trans_start) {
        vkCmdBindPipeline(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _scene.transparent_pipeline_info.pipeline);
      }

      if (material_desc_set != draw_obj.mat_desc_set) {
        material_desc_set = draw_obj.mat_desc_set;

        vkCmdBindDescriptorSets(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_layout, 1, 1,
                                &draw_obj.mat_desc_set, 0, nullptr);
      }

      vkCmdBindDescriptorSets(secondary_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline_layout, 2, 1,
                              &draw_obj.obj_desc_set, 0, nullptr);

      vkCmdBindIndexBuffer(secondary_buf, draw_obj.index_buffer, 0, VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexed(secondary_buf, draw_obj.indices_count, 1, draw_obj.indices_start, 0, 0);
    }

    vkEndCommandBuffer(secondary_buf);

    return secondary_buf;
  };

  std::vector<std::future<VkCommandBuffer>> futures;

  uint32_t thread_count = std::thread::hardware_concurrency() - 1;
  uint32_t primitive_len = _scene.draw_objects.size();
  uint32_t stride = primitive_len / thread_count;

  if (stride * thread_count < primitive_len) {
    stride++;
  }

  stride = 500;
  uint32_t secondary_buf_idx = 0;
  for (uint32_t i = 0; i < primitive_len; i += stride) {

    auto future = std::async(std::launch::deferred, draw, _scene.draw_objects, i, stride, secondary_buf_idx);

    futures.push_back(std::move(future));
    secondary_buf_idx++;
  }
  for (auto &future : futures) {
    VkCommandBuffer buf = future.get();
    vkCmdExecuteCommands(cmd_buf, 1, &buf);
  }

  vkCmdEndRendering(cmd_buf);
}

void VkBackend::render_ui(VkCommandBuffer cmd_buf, VkExtent2D extent, VkImageView image_view) {

  // load last render(s). aka, don't delete the scene when rendering ui.
  VkRenderingAttachmentInfo color_attachment =
      create_color_attachment_info(image_view, nullptr, VK_ATTACHMENT_LOAD_OP_LOAD);

  VkRenderingInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{
      .offset = VkOffset2D{0, 0},
      .extent = extent,
  };
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.layerCount = 1;
  rendering_info.pStencilAttachment = nullptr;
  // rendering_info.pDepthAttachment = &depth_attachment;

  vkCmdBeginRendering(cmd_buf, &rendering_info);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);

  vkCmdEndRendering(cmd_buf);
}

MeshBuffers VkBackend::upload_mesh_buffers(std::span<uint32_t> indices, std::span<Vertex> vertices) {
  const size_t vertex_buffer_bytes = vertices.size() * sizeof(Vertex);
  const size_t index_buffer_bytes = indices.size() * sizeof(uint32_t);

  AllocatedBuffer staging_buf =
      create_buffer(vertex_buffer_bytes + index_buffer_bytes, _allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

  void *staging_data = staging_buf.allocation->GetMappedData();

  // share staging buffer for vertices and indices
  memcpy(staging_data, vertices.data(), vertex_buffer_bytes);
  memcpy((char *)staging_data + vertex_buffer_bytes, indices.data(), index_buffer_bytes);

  MeshBuffers new_mesh_buffer;
  new_mesh_buffer.vertices = create_buffer(vertex_buffer_bytes, _allocator,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY, 0);
  new_mesh_buffer.indices =
      create_buffer(index_buffer_bytes, _allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_GPU_ONLY, 0);

  immediate_submit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_buffer_region{};
    vertex_buffer_region.size = vertex_buffer_bytes;
    vertex_buffer_region.srcOffset = 0;
    vertex_buffer_region.dstOffset = 0;

    vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.vertices.buffer, 1, &vertex_buffer_region);

    VkBufferCopy index_buffer_region{};
    index_buffer_region.size = index_buffer_bytes;
    index_buffer_region.srcOffset = vertex_buffer_bytes;
    index_buffer_region.dstOffset = 0;

    vkCmdCopyBuffer(cmd, staging_buf.buffer, new_mesh_buffer.indices.buffer, 1, &index_buffer_region);
  });

  destroy_buffer(_allocator, staging_buf);
  return new_mesh_buffer;
}

AllocatedImage VkBackend::upload_texture_image(void *data, VkImageUsageFlags usage, uint32_t height, uint32_t width) {
  VkExtent2D extent{.width = width, .height = height};

  uint32_t data_size = width * height * sizeof(uint32_t);

  AllocatedBuffer staging_buf = create_buffer(data_size, _allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                              VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

  memcpy(staging_buf.allocation->GetMappedData(), data, data_size);

  AllocatedImage new_texture;
  new_texture = create_image(_device_context.logical_device, _allocator, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             extent, VK_FORMAT_R8G8B8A8_UNORM);

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

    vkCmdCopyBufferToImage(cmd, staging_buf.buffer, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    insert_image_memory_barrier(cmd, new_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });
  destroy_buffer(_allocator, staging_buf);

  return new_texture;
}

void VkBackend::destroy() {

  vkDeviceWaitIdle(_device_context.logical_device);
  DEBUG_PRINT("destroying Vulkan Backend");

  _deletion_queue.flush();

  if constexpr (use_validation_layers) {
    _debugger.destroy();
  }

  for (Frame &frame : _frames) {
    frame.destroy();
    destroy_buffer(_allocator, frame.scene_data_buffer);
  }

  destroy_scene(this, _scene);

  ImGui_ImplGlfw_Shutdown();
  ImGui_ImplVulkan_Shutdown();

  ImGui::DestroyContext();

  vkDestroyDescriptorPool(_device_context.logical_device, _imm_descriptor_pool, nullptr);

  destroy_image(_device_context.logical_device, _allocator, _draw_image);
  destroy_image(_device_context.logical_device, _allocator, _depth_image);
  destroy_image(_device_context.logical_device, _allocator, _default_texture);

  vkDestroySampler(_device_context.logical_device, _default_nearest_sampler, nullptr);
  vkDestroySampler(_device_context.logical_device, _default_linear_sampler, nullptr);
  vkDestroyFence(_device_context.logical_device, _imm_fence, nullptr);

  vmaDestroyAllocator(_allocator);

  _imm_cmd_context.destroy();
  _swapchain_context.destroy();
  _device_context.destroy();

  vkDestroyInstance(_instance, nullptr);
}
