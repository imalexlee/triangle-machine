#pragma once

#include <array>
#include <vk_backend/vk_types.h>

class Debugger {
public:
  VkDebugUtilsMessengerEXT messenger;

  VkResult create(VkInstance& instance);
  void destroy();
  VkDebugUtilsMessengerCreateInfoEXT create_messenger_info();
  VkValidationFeaturesEXT create_validation_features();
  std::array<const char*, 1> create_validation_layers();

private:
  static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
  DeletionQueue _deletion_queue;
};
