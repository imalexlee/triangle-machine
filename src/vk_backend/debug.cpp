#include "debug.h"
#include <iostream>
#include <vulkan/vulkan_core.h>

VkBool32 Debugger::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

  // only print info, warnings, and errors
  if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  }
  return VK_FALSE;
};

VkResult Debugger::create(VkInstance instance) {

  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  VkDebugUtilsMessengerCreateInfoEXT messenger_ci = create_debug_info();

  if (func != nullptr) {
    return func(instance, &messenger_ci, nullptr, &messenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

VkDebugUtilsMessengerCreateInfoEXT Debugger::create_debug_info() {
  VkDebugUtilsMessengerCreateInfoEXT messenger_ci = {};
  messenger_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_ci.pfnUserCallback = debug_callback;
  return messenger_ci;
}

void Debugger::destroy(VkInstance instance) {
  fmt::println("destroying debugger");
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, messenger, nullptr);
  }
}
