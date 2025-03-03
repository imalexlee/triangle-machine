#include "vk_debug.h"
#include <array>
#include <iostream>
#include <vulkan/vulkan_core.h>

static constexpr std::array enabled_validation_features{
    // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
};

static constexpr std::array disabled_validation_features{
    VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT,
};

static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                        [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData) {

    // only print info, warnings, and errors
    if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        std::cout << pCallbackData->pMessage << "\n\n";
    }
    return VK_FALSE;
}

VkResult debugger_init(Debugger* db, VkInstance instance, VkDevice device) {

    db->logical_device = device;

    auto msg_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    VkDebugUtilsMessengerCreateInfoEXT messenger_ci = vk_messenger_info_create();

    if (msg_fn != nullptr) {

        db->obj_name_pfn = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));

        VkResult result = msg_fn(instance, &messenger_ci, nullptr, &db->messenger);

        return result;
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VkDebugUtilsMessengerCreateInfoEXT vk_messenger_info_create() {
    VkDebugUtilsMessengerCreateInfoEXT messenger_ci = {};
    messenger_ci.sType                              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    messenger_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

    messenger_ci.pfnUserCallback = debug_callback;

    return messenger_ci;
}

VkValidationFeaturesEXT vk_validation_features_create() {
    VkValidationFeaturesEXT validation_features{};
    validation_features.sType                          = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.pEnabledValidationFeatures     = enabled_validation_features.data();
    validation_features.enabledValidationFeatureCount  = enabled_validation_features.size();
    validation_features.pDisabledValidationFeatures    = disabled_validation_features.data();
    validation_features.disabledValidationFeatureCount = disabled_validation_features.size();
    return validation_features;
}

void debugger_deinit(const Debugger* db, VkInstance instance) {
    DEBUG_PRINT("Destroying Debugger");
    auto destroy_messenger_fn =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy_messenger_fn != nullptr) {
        destroy_messenger_fn(instance, db->messenger, nullptr);
    }
}
