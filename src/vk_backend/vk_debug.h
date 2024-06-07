#pragma once

#include <array>
#include <cstdint>
#include <vk_backend/vk_types.h>

class Debugger {
public:
  VkDebugUtilsMessengerEXT messenger;
  PFN_vkSetDebugUtilsObjectNameEXT obj_name_fn;

  VkResult create(VkInstance instance, VkDevice device);
  void destroy();

  VkDebugUtilsMessengerCreateInfoEXT create_messenger_info();
  VkValidationFeaturesEXT create_validation_features();
  std::array<const char*, 1> create_validation_layers();

  template <typename T> void set_handle_name(T handle, VkObjectType object_type, std::string name) {
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = object_type;
    name_info.objectHandle = (uint64_t)handle;
    name_info.pObjectName = name.data();
    obj_name_fn(logical_device, &name_info);
  };

private:
  static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                 void* pUserData);
  DeletionQueue _deletion_queue;
  VkDevice logical_device;
};
