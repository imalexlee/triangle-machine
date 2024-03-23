#include "vk_backend/vk_backend.h"
#include "core/window.h"
#include "fmt/base.h"
#include "vk_backend/vk_types.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#ifdef NDEBUG
constexpr bool use_validation_layers = false;
#else
constexpr bool use_validation_layers = true;
#endif // NDEBUG

void VkBackend::init(Window& window) {
  create_instance(window.glfw_window);
  if (use_validation_layers) {
    _debugger.create(_instance);
  }
  VkSurfaceKHR surface = window.get_surface(_instance);
  _device_context.create(_instance, surface);
  _swapchain_context.create(_instance, _device_context, surface, window.width, window.height, VK_PRESENT_MODE_FIFO_KHR);
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

void VkBackend::cleanup() {
  fmt::println("destroying Vulkan instance");
  if constexpr (use_validation_layers) {
    _debugger.destroy();
  }

  _swapchain_context.destroy();
  _device_context.destroy();

  vkDestroyInstance(_instance, nullptr);
}
