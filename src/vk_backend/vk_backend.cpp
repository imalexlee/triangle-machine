#include "vk_backend/resources/vk_allocate.h"

#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_backend.h"
#include "vk_backend/vk_sync.h"
#include "vk_mem_alloc.h"

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
  _allocator = create_allocator(_instance, _device_context);
  _swapchain_context.create(_instance, _device_context, surface, window.width, window.height, VK_PRESENT_MODE_FIFO_KHR);

  for (Frame& frame : _frames) {
    frame.create(_device_context.logical_device, _device_context.queues.graphics_family_index);
  }

  VkExtent2D image_extent{
      .width = window.width,
      .height = window.height,
  };

  _draw_image =
      create_image(_device_context.logical_device, _allocator,
                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   image_extent, VK_FORMAT_R16G16B16A16_SFLOAT);

  if (use_validation_layers) {
    _debugger.create(_instance);
  }
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

  Frame& current_frame = _frames[get_frame_index()];
  auto frame_indexxx = get_frame_index();
  VkCommandBuffer cmd_buffer = current_frame.command_context.primary_buffer;

  // wait for previous command buffer to finish executing
  VK_CHECK(vkWaitForFences(_device_context.logical_device, 1, &current_frame.render_fence, VK_TRUE, TIMEOUT_DURATION));

  uint32_t swapchain_image_index;
  VK_CHECK(vkAcquireNextImageKHR(_device_context.logical_device, _swapchain_context.swapchain, TIMEOUT_DURATION,
                                 current_frame.present_semaphore, nullptr, &swapchain_image_index));

  VK_CHECK(vkResetFences(_device_context.logical_device, 1, &current_frame.render_fence));

  current_frame.command_context.begin_primary_buffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VkImageMemoryBarrier2 image_barrier = create_image_memory_barrier(_swapchain_context.images[swapchain_image_index],
                                                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  insert_image_memory_barrier(cmd_buffer, image_barrier);

  VkClearColorValue clear_color = {{0, 1, 1, 1}};

  VkImageSubresourceRange subresource_range = create_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  vkCmdClearColorImage(cmd_buffer, _swapchain_context.images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL,
                       &clear_color, 1, &subresource_range);

  image_barrier = create_image_memory_barrier(_swapchain_context.images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL,
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  insert_image_memory_barrier(cmd_buffer, image_barrier);

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

  VK_CHECK(vkQueuePresentKHR(_device_context.queues.present, &present_info));

  _frame_num++;
}

// void VkBackend::draw_geometry(VkCommandBuffer cmd_buf, VkExtent2D extent, uint32_t swapchain_img_idx) {
//   VkRenderingInfo rendering_info{};
//   VkRenderingAttachmentInfo color_attachment =
//       create_rendering_attachment(_swapchain_context.image_views[swapchain_img_idx], nullptr);
//   rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
//   rendering_info.renderArea = VkRect2D{
//       .offset = VkOffset2D{0, 0},
//       .extent = extent,
//   };
//   rendering_info.pDepthAttachment = depth_attachment
// }

void VkBackend::destroy() {
  vkDeviceWaitIdle(_device_context.logical_device);
  DEBUG_PRINT("destroying Vulkan Backend");
  if constexpr (use_validation_layers) {
    _debugger.destroy();
  }

  for (Frame& frame : _frames) {
    frame.destroy();
  }
  _swapchain_context.destroy();

  destroy_image(_device_context.logical_device, _allocator, _draw_image);
  vmaDestroyAllocator(_allocator);

  _device_context.destroy();
  vkDestroyInstance(_instance, nullptr);
}
