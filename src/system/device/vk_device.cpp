#include "vk_device.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <set>
#include <string.h>
#include <vector>

VkPhysicalDevice vk_physical_device_create(VkInstance instance) {
    std::vector<VkPhysicalDevice> physical_devices;
    uint32_t                      physical_device_count;

    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
    physical_devices.resize(physical_device_count);

    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    VkPhysicalDevice physical_device = nullptr;
    for (VkPhysicalDevice& device : physical_devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        // needs to be at least Vulkan 1.3 for sync 2 and dynamic rendering features
        if (properties.apiVersion < VK_API_VERSION_1_3) {
            continue;
        }

        // prefer dedicated GPU if we find one
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return device;
        }
        physical_device = device;
    }

    if (physical_device) {
        return physical_device;
    }

    std::cout << "Cannot find a suitable GPU!" << std::endl;
    exit(EXIT_FAILURE);
}

DeviceQueueFamilies vk_queue_families_get(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {

    std::vector<VkQueueFamilyProperties> queue_family_properties;
    uint32_t                             family_property_count;

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, nullptr);
    queue_family_properties.resize(family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, queue_family_properties.data());

    std::optional<uint32_t> graphics_index;
    std::optional<uint32_t> present_index;

    // find graphics and presentation queues. prefer the same queue
    for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_supported);
        // iterate through all queues and hope to find one queue family that supports both.
        // Otherwise, pick out queues that either present or graphics can use.
        bool graphics_supported = queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        if (present_supported && graphics_supported) {
            graphics_index = i;
            present_index  = i;
            break;
        }
        if (present_supported) {
            present_index = i;
        } else if (graphics_supported) {
            graphics_index = i;
        }
    }

    if (!present_index.has_value() || !graphics_index.has_value()) {
        std::cout << "Cannot find suitable queues!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::optional<uint32_t> compute_index;

    // grab a compute queue index. prefer a different index than the graphics queue
    for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
        bool compute_supported = queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
        if (!compute_supported) {
            continue;
        }
        if (!compute_index.has_value()) {
            compute_index = i;
        }
        if (i != graphics_index.value()) {
            compute_index = i;
            break;
        }
    }

    DeviceQueueFamilies queue_families{};
    queue_families.graphics = graphics_index.value();
    queue_families.present  = present_index.value();
    queue_families.compute  = compute_index.value();

    return queue_families;
}

VkDevice vk_logical_device_create(VkPhysicalDevice physical_device, const DeviceQueueFamilies* queue_families) {
    std::set                             unique_family_indices{queue_families->graphics, queue_families->present, queue_families->compute};
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    constexpr float                      priority = 1.f;

    for (const uint32_t& index : unique_family_indices) {
        VkDeviceQueueCreateInfo queue_ci{};
        queue_ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.pNext            = nullptr;
        queue_ci.queueCount       = 1;
        queue_ci.queueFamilyIndex = index;
        queue_ci.pQueuePriorities = &priority;
        queue_infos.push_back(queue_ci);
    }

    constexpr std::array device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
    };

    VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features{};
    ray_query_features.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    ray_query_features.rayQuery = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_struct_features{};
    accel_struct_features.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel_struct_features.accelerationStructure = VK_TRUE;
    accel_struct_features.pNext                 = &ray_query_features;

    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_feature{};
    vertex_input_feature.sType                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    vertex_input_feature.vertexInputDynamicState = VK_TRUE;
    vertex_input_feature.pNext                   = &accel_struct_features;

    VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_feature{};
    shader_object_feature.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shader_object_feature.shaderObject = VK_TRUE;
    shader_object_feature.pNext        = &vertex_input_feature;

    VkPhysicalDeviceVulkan13Features features_1_3{};
    features_1_3.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features_1_3.synchronization2 = VK_TRUE;
    features_1_3.dynamicRendering = VK_TRUE;
    features_1_3.pNext            = &shader_object_feature;

    VkPhysicalDeviceVulkan12Features features_1_2{};
    features_1_2.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features_1_2.descriptorBindingVariableDescriptorCount     = VK_TRUE;
    features_1_2.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
    features_1_2.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features_1_2.descriptorBindingUpdateUnusedWhilePending    = VK_TRUE;
    features_1_2.descriptorBindingPartiallyBound              = VK_TRUE;
    features_1_2.bufferDeviceAddress                          = VK_TRUE;
    features_1_2.descriptorIndexing                           = VK_TRUE;
    features_1_2.runtimeDescriptorArray                       = VK_TRUE;
    features_1_2.shaderStorageImageArrayNonUniformIndexing    = VK_TRUE;
    features_1_2.pNext                                        = &features_1_3;

    VkPhysicalDeviceFeatures features_1_0{};
    features_1_0.shaderStorageImageExtendedFormats = VK_TRUE;

    VkDeviceCreateInfo device_ci{};
    device_ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.pQueueCreateInfos       = queue_infos.data();
    device_ci.queueCreateInfoCount    = queue_infos.size();
    device_ci.ppEnabledExtensionNames = device_extensions.data();
    device_ci.enabledExtensionCount   = device_extensions.size();
    device_ci.pEnabledFeatures        = &features_1_0;
    device_ci.pNext                   = &features_1_2;

    VkDevice device;
    VK_CHECK(vkCreateDevice(physical_device, &device_ci, nullptr, &device));

    return device;
}

DeviceQueues vk_device_queues_create(VkDevice device, const DeviceQueueFamilies* queue_families) {
    DeviceQueues queues{};
    vkGetDeviceQueue(device, queue_families->graphics, 0, &queues.graphics);
    vkGetDeviceQueue(device, queue_families->present, 0, &queues.present);
    vkGetDeviceQueue(device, queue_families->compute, 0, &queues.compute);
    return queues;
}