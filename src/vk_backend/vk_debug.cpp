#include "vk_debug.h"
#include <array>
#include <iostream>
#include <vulkan/vulkan_core.h>

static constexpr std::array<VkValidationFeatureEnableEXT, 3> enabled_validation_features{
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
    //    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    VkValidationFeatureEnableEXT::VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
};
static constexpr std::array<const char*, 1> validation_layers{"VK_LAYER_KHRONOS_validation"};

VkBool32 Debugger::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

  // only print info, warnings, and errors
  if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  }
  return VK_FALSE;
};

VkResult Debugger::create(VkInstance& instance) {

  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  VkDebugUtilsMessengerCreateInfoEXT messenger_ci = create_messenger_info();

  if (func != nullptr) {
    VkResult result = func(instance, &messenger_ci, nullptr, &messenger);
    if (result == VK_SUCCESS) {
      _deletion_queue.push_persistant([&]() {
        DEBUG_PRINT("destroying debugger");
        auto func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
          func(instance, messenger, nullptr);
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
