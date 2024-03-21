#include "vk_backend/vk_backend.h"
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
constexpr std::array<const char*, 1> validation_layers{"VK_LAYER_KHRONOS_validation"};
constexpr std::array<VkValidationFeatureEnableEXT, 4> enabled_validation_features{
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
};
#endif // NDEBUG
constexpr std::array<VkValidationFeatureDisableEXT, 1> disabled_validation_features{
    VkValidationFeatureDisableEXT::VK_VALIDATION_FEATURE_DISABLE_ALL_EXT};
constexpr std::array<const char*, 2> device_extensions{
    "VK_KHR_dynamic_rendering",
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

void VkBackend::init(GLFWwindow* window) {
  create_instance(window);
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
  validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

#ifdef NDEBUG

  validation_features.disabledValidationFeatureCount = disabled_validation_features.size();
  validation_features.pDisabledValidationFeatures = disabled_validation_features.data();
#else

  debug_ci = _debugger.create_debug_info();

  validation_features.pEnabledValidationFeatures = enabled_validation_features.data();
  validation_features.enabledValidationFeatureCount = enabled_validation_features.size();

  validation_features.pNext = &debug_ci;
  instance_ci.pNext = &validation_features;
  instance_ci.enabledLayerCount = validation_layers.size();
  instance_ci.ppEnabledLayerNames = validation_layers.data();
#endif // NDEBUG

  VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &_instance));
}

std::vector<const char*> VkBackend::get_instance_extensions(GLFWwindow* window) {

  uint32_t count{0};
  const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char*> extensions;
  for (size_t i = 0; i < count; i++) {
    extensions.emplace_back(glfw_extensions[i]);
  }
  if (use_validation_layers) {
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

void VkBackend::cleanup() {
  fmt::println("destroying Vulkan instance");
  if (use_validation_layers) {
    _debugger.destroy(_instance);
  }
  vkDestroyInstance(_instance, nullptr);
}
