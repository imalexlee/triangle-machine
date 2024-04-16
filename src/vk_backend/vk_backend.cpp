#include "vk_backend/vk_pipeline.h"
#include <chrono>
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

void VkBackend::create(Window& window) {
  create_instance(window.glfw_window);
  VkSurfaceKHR surface = window.get_vulkan_surface(_instance);

  _device_context.create(_instance, surface);
  _swapchain_context.create(_instance, _device_context, surface, window.width, window.height, VK_PRESENT_MODE_FIFO_KHR);
  create_allocator();

  for (Frame& frame : _frames) {
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
  create_default_data();
  create_pipelines();

  if (use_validation_layers) {
    _debugger.create(_instance);
  }
}

void VkBackend::create_default_data() { _scene = load_scene(this, "../../assets/3d/porsche_large.glb"); }

void VkBackend::update_scene() {
  using namespace std::chrono;
  static auto t1 = high_resolution_clock::now();
  auto time_span = duration_cast<duration<float>>(high_resolution_clock::now() - t1);

  glm::mat4 upside_down = glm::mat4{1.f};
  upside_down[1][1] *= -1;

  glm::vec3 cam_pos = {0, 1, -8};

  glm::mat4 model = upside_down * glm::rotate(glm::mat4{1.f}, glm::radians(time_span.count() * 30), glm::vec3{0, 1, 0});
  glm::mat4 view = glm::translate(glm::mat4(1.f), cam_pos);
  glm::mat4 projection = glm::perspective(
      glm::radians(60.f), (float)_swapchain_context.extent.width / (float)_swapchain_context.extent.height, 10000.0f,
      0.1f);

  _scene_data.view_proj = projection * view * model;
  _scene.update(0);
}

void VkBackend::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {

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

void VkBackend::create_instance(GLFWwindow* window) {

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "awesome app";
  app_info.pEngineName = "awesome engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  std::vector<const char*> instance_extensions = get_instance_extensions(window);

  VkInstanceCreateInfo instance_ci{};
  instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_ci.pApplicationInfo = &app_info;
  instance_ci.flags = 0;
  instance_ci.ppEnabledExtensionNames = instance_extensions.data();
  instance_ci.enabledExtensionCount = instance_extensions.size();

  VkDebugUtilsMessengerCreateInfoEXT debug_ci;
  VkValidationFeaturesEXT validation_features;
  std::array<const char*, 1> validation_layers;

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

  // cosider passing the shader locations in from the renderer instead of here
  VkShaderModule vert_shader =
      load_shader_module(_device_context.logical_device, "../../shaders/vertex/indexed_triangle.vert.glsl.spv");
  VkShaderModule frag_shader =
      load_shader_module(_device_context.logical_device, "../../shaders/fragment/triangle.frag.glsl.spv");

  builder.set_shader_stages(vert_shader, frag_shader);
  builder.disable_blending();
  builder.set_input_assembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_raster_culling(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
  builder.set_raster_poly_mode(VK_POLYGON_MODE_FILL);
  builder.set_multisample_state(VK_SAMPLE_COUNT_1_BIT);
  builder.set_depth_stencil_state(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_render_info(_swapchain_context.format, _depth_image.image_format);

  VkPushConstantRange push_constant_range{};
  push_constant_range.size = sizeof(DrawObjectPushConstants);
  push_constant_range.offset = 0;
  push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  std::array<VkPushConstantRange, 1> push_constant_ranges{push_constant_range};
  // all frames have the same layout so you can use the first one's layout
  std::array<VkDescriptorSetLayout, 1> set_layoutrs{_frames[0].desc_set_layout};

  builder.set_layout(set_layoutrs, push_constant_ranges, 0);

  PipelineInfo new_pipeline_info = builder.build_pipeline(_device_context.logical_device);

  _scene.opaque_pipeline_info = std::make_shared<PipelineInfo>(new_pipeline_info);

  _deletion_queue.push_persistant([=, this]() {
    vkDestroyShaderModule(_device_context.logical_device, vert_shader, nullptr);
    vkDestroyShaderModule(_device_context.logical_device, frag_shader, nullptr);
  });
}

std::vector<const char*> VkBackend::get_instance_extensions(GLFWwindow* window) {
  uint32_t count{0};
  const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char*> extensions;
  for (size_t i = 0; i < count; i++) {
    extensions.emplace_back(glfw_extensions[i]);
  }
  if constexpr (use_validation_layers) {
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

void VkBackend::draw() {
  update_scene();

  Frame& current_frame = get_current_frame();
  VkCommandBuffer cmd_buffer = current_frame.command_context.primary_buffer;

  // wait for previous command buffer to finish executing
  VK_CHECK(vkWaitForFences(_device_context.logical_device, 1, &current_frame.render_fence, VK_TRUE, TIMEOUT_DURATION));

  uint32_t swapchain_image_index;
  VkResult result =
      vkAcquireNextImageKHR(_device_context.logical_device, _swapchain_context.swapchain, TIMEOUT_DURATION,
                            current_frame.present_semaphore, nullptr, &swapchain_image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return;
  }

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &current_frame.render_fence));

  current_frame.command_context.begin_primary_buffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  insert_image_memory_barrier(cmd_buffer, _swapchain_context.images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkClearColorValue clear_color{{0, 0, 0, 1}};
  VkImageSubresourceRange sub_range = create_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  vkCmdClearColorImage(cmd_buffer, _swapchain_context.images[swapchain_image_index],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &sub_range);

  insert_image_memory_barrier(cmd_buffer, _swapchain_context.images[swapchain_image_index],
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  insert_image_memory_barrier(cmd_buffer, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  draw_geometry(cmd_buffer, _swapchain_context.extent, swapchain_image_index);

  insert_image_memory_barrier(cmd_buffer, _swapchain_context.images[swapchain_image_index],
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
}

void VkBackend::resize(uint32_t width, uint32_t height) {
  vkDeviceWaitIdle(_device_context.logical_device);
  _swapchain_context.reset_swapchain(_device_context, width, height);

  destroy_image(_device_context.logical_device, _allocator, _depth_image);
  _depth_image = create_image(_device_context.logical_device, _allocator, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              _swapchain_context.extent, VK_FORMAT_D32_SFLOAT);

  for (Frame& frame : _frames) {
    frame.reset_sync_structures(_device_context.logical_device);
  }
}

void VkBackend::draw_geometry(VkCommandBuffer cmd_buf, VkExtent2D extent, uint32_t swapchain_img_idx) {

  // make this use _draw_image.image after blit image is done
  VkRenderingAttachmentInfo color_attachment =
      create_color_attachment_info(_swapchain_context.image_views[swapchain_img_idx], nullptr);

  VkRenderingAttachmentInfo depth_attachment = create_depth_attachment_info(_depth_image.image_view);

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
  rendering_info.pDepthAttachment = &depth_attachment;

  vkCmdBeginRendering(cmd_buf, &rendering_info);

  get_current_frame().clear_scene_desc_set(_device_context.logical_device);
  // allocate an empty set with the layout to hold our global scene data
  VkDescriptorSet scene_desc_set = get_current_frame().create_scene_desc_set(_device_context.logical_device);

  // fill in the buffer with the updated scene data
  SceneData* scene_data = (SceneData*)get_current_frame().scene_data_buffer.allocation->GetMappedData();
  *scene_data = _scene_data;

  // connect buffer that was just filled to a binding then hook it up to the allocated desc set
  DescriptorWriter writer;
  writer.write_buffer(0, get_current_frame().scene_data_buffer.buffer, sizeof(SceneData), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(_device_context.logical_device, scene_desc_set);
  writer.clear();

  auto draw = [&](const DrawNode& draw_node) {
    // TODO: attach either opaque or transparent pipeline based on primitive flag
    for (Primitive& primitive : draw_node.mesh.value()->primitives) {
      vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _scene.opaque_pipeline_info->pipeline);
      const auto mesh = draw_node.mesh->get();

      VkViewport viewport{};
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.width = static_cast<float>(_swapchain_context.extent.width);
      viewport.height = static_cast<float>(_swapchain_context.extent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.extent = _swapchain_context.extent;
      scissor.offset = {0, 0};
      vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

      vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _scene.opaque_pipeline_info->pipeline_layout, 0,
                              1, &scene_desc_set, 0, nullptr);

      DrawObjectPushConstants push_constants{
          .local_transform = draw_node.local_transform,
          .vertex_buffer_address = mesh->buffers.vertex_buffer_address,
      };

      vkCmdPushConstants(cmd_buf, _scene.opaque_pipeline_info->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(DrawObjectPushConstants), &push_constants);

      vkCmdBindIndexBuffer(cmd_buf, mesh->buffers.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

      uint32_t indices_count = mesh->buffers.indices.info.size / sizeof(uint32_t);
      vkCmdDrawIndexed(cmd_buf, indices_count, 1, primitive.indices_start, 0, 0);
    }
  };

  for (const DrawNode& node : _scene.draw_ctx.opaque_nodes) {
    draw(node);
  }

  vkCmdEndRendering(cmd_buf);
}

MeshBuffers VkBackend::upload_mesh_buffers(std::span<uint32_t> indices, std::span<Vertex> vertices) {
  const size_t vertex_buffer_bytes = vertices.size() * sizeof(Vertex);
  const size_t index_buffer_bytes = indices.size() * sizeof(uint32_t);

  AllocatedBuffer staging_buf =
      create_buffer(vertex_buffer_bytes + index_buffer_bytes, _allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

  void* staging_data = staging_buf.allocation->GetMappedData();

  // share staging buffer for vertices and indices
  memcpy(staging_data, vertices.data(), vertex_buffer_bytes);
  memcpy((char*)staging_data + vertex_buffer_bytes, indices.data(), index_buffer_bytes);

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

void VkBackend::destroy() {
  vkDeviceWaitIdle(_device_context.logical_device);
  DEBUG_PRINT("destroying Vulkan Backend");
  _deletion_queue.flush();

  if constexpr (use_validation_layers) {
    _debugger.destroy();
  }

  for (Frame& frame : _frames) {
    frame.destroy();
    destroy_buffer(_allocator, frame.scene_data_buffer);
  }

  destroy_scene(_device_context.logical_device, _allocator, _scene);
  destroy_image(_device_context.logical_device, _allocator, _draw_image);
  destroy_image(_device_context.logical_device, _allocator, _depth_image);

  vmaDestroyAllocator(_allocator);

  vkDestroyFence(_device_context.logical_device, _imm_fence, nullptr);

  _imm_cmd_context.destroy();
  _swapchain_context.destroy();
  _device_context.destroy();

  vkDestroyInstance(_instance, nullptr);
}
