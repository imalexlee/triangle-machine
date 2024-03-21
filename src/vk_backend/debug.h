#pragma once
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

// Sets up debug information to be used with validation layers
class Debugger {
public:
  VkDebugUtilsMessengerEXT messenger;

  VkResult create(VkInstance instance);
  void destroy(VkInstance instance);
  VkDebugUtilsMessengerCreateInfoEXT create_debug_info();

private:
  static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
};
