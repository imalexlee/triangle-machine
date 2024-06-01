#include "vk_debug.h"
#include <array>
#include <iostream>
#include <vulkan/vulkan_core.h>

#ifdef NDEBUG

#endif

static constexpr std::array<VkValidationFeatureEnableEXT, 3> enabled_validation_features{
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
};
static constexpr std::array<const char*, 1> validation_layers{"VK_LAYER_KHRONOS_validation"};

VkBool32 Debugger::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                  [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                  [[maybe_unused]] void* pUserData) {

  // only print info, warnings, and errors
  if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    std::cerr << "\n";
  }
  return VK_FALSE;
};

VkResult Debugger::create(VkInstance instance, VkDevice device) {

  logical_device = device;

  auto msg_fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  VkDebugUtilsMessengerCreateInfoEXT messenger_ci = create_messenger_info();

  if (msg_fn != nullptr) {

    obj_name_fn = (PFN_vkSetDebugUtilsObjectNameEXT)(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));

    VkResult result = msg_fn(instance, &messenger_ci, nullptr, &messenger);
    if (result == VK_SUCCESS) {
      _deletion_queue.push_persistant([=, this]() {
        DEBUG_PRINT("destroying debugger");
        auto destroy_messenger_fn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_messenger_fn != nullptr) {
          destroy_messenger_fn(instance, messenger, nullptr);
        }
      });
    }

    return result;
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

VkDebugUtilsMessengerCreateInfoEXT Debugger::create_messenger_info() {
  VkDebugUtilsMessengerCreateInfoEXT messenger_ci = {};
  messenger_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  messenger_ci.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

  messenger_ci.pfnUserCallback = debug_callback;

  return messenger_ci;
}

VkValidationFeaturesEXT Debugger::create_validation_features() {
  VkValidationFeaturesEXT validation_features{};
  validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  validation_features.pEnabledValidationFeatures = enabled_validation_features.data();
  validation_features.enabledValidationFeatureCount = enabled_validation_features.size();
  return validation_features;
}

std::array<const char*, 1> Debugger::create_validation_layers() { return validation_layers; }

void Debugger::destroy() { _deletion_queue.flush(); }
