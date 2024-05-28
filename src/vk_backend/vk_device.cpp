#include "vk_device.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <set>
#include <strings.h>
#include <vector>

void DeviceContext::create(VkInstance instance, VkSurfaceKHR surface) {
  create_physical_device(instance);
  get_queue_family_indices(surface);
  create_logical_device();
}

void DeviceContext::create_physical_device(VkInstance instance) {
  std::vector<VkPhysicalDevice> physical_devices;
  uint32_t physical_device_count;

  VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
  physical_devices.resize(physical_device_count);

  VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

  for (VkPhysicalDevice& device : physical_devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);

    // needs to be at least Vulkan 1.3 for sync 2 and dynamic rendering features
    if (properties.apiVersion < VK_API_VERSION_1_3) {
      continue;
    }

    // use up to 2x2 msaa sampling
    msaa_sample_count = std::clamp(properties.limits.framebufferColorSampleCounts, 1u, 4u);

    // prefer dedicated GPU if we find one
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      physical_device = device;
      return;
    }
    physical_device = device;
  }

  if (!physical_device) {
    std::cout << "Cannot find a suitable GPU!" << std::endl;
    exit(EXIT_FAILURE);
  }
}

void DeviceContext::destroy() { vkDestroyDevice(logical_device, nullptr); }

void DeviceContext::get_queue_family_indices(VkSurfaceKHR surface) {
  std::vector<VkQueueFamilyProperties> queue_family_properties;
  uint32_t family_property_count;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, nullptr);
  queue_family_properties.resize(family_property_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_property_count, queue_family_properties.data());

  std::optional<uint32_t> graphics_index;
  std::optional<uint32_t> present_index;

  // attempt to use the same queue family for graphics and presentation operations
  for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
    VkBool32 present_supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_supported);
    // iterate through all queues and hope to find one queue family that supports both.
    // otherwise, pick out queues that either present or graphics can use.
    if (present_supported && queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_index = i;
      present_index = i;
      break;
    } else if (present_supported) {
      present_supported = i;
    } else if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_index = i;
    }
  }

  if (!present_index.has_value() || !graphics_index.has_value()) {
    std::cout << "Cannot find suitable queues!" << std::endl;
    exit(EXIT_FAILURE);
  }

  queues.graphics_family_index = graphics_index.value();
  queues.present_family_index = present_index.value();
}

void DeviceContext::create_logical_device() {
  std::set<uint32_t> unique_family_indices{queues.graphics_family_index, queues.present_family_index};
  std::vector<VkDeviceQueueCreateInfo> queue_infos;

  constexpr float priority = 1.f;

  for (const uint32_t& index : unique_family_indices) {
    VkDeviceQueueCreateInfo queue_ci{};
    queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.pNext = nullptr;
    queue_ci.queueCount = 1;
    queue_ci.queueFamilyIndex = index;
    queue_ci.pQueuePriorities = &priority;
    queue_infos.push_back(queue_ci);
  }
  constexpr std::array<const char*, 2> device_extensions{
      "VK_KHR_dynamic_rendering",
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  // so that secondary command buffers can inherit the state of the viewport and scissor during rendering
  VkPhysicalDeviceInheritedViewportScissorFeaturesNV inherited_scissor_feature{};
  inherited_scissor_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV;
  inherited_scissor_feature.inheritedViewportScissor2D = VK_TRUE;

  VkPhysicalDeviceVulkan13Features features_1_3{};
  features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features_1_3.synchronization2 = VK_TRUE;
  features_1_3.dynamicRendering = VK_TRUE;
  features_1_3.pNext = &inherited_scissor_feature;

  VkPhysicalDeviceVulkan12Features features_1_2{};
  features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features_1_2.bufferDeviceAddress = VK_TRUE;
  features_1_2.descriptorIndexing = VK_TRUE;
  features_1_2.pNext = &features_1_3;

  VkDeviceCreateInfo device_ci{};
  device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_ci.pQueueCreateInfos = queue_infos.data();
  device_ci.queueCreateInfoCount = queue_infos.size();
  device_ci.ppEnabledExtensionNames = device_extensions.data();
  device_ci.enabledExtensionCount = device_extensions.size();
  device_ci.pNext = &features_1_2;

  VK_CHECK(vkCreateDevice(physical_device, &device_ci, nullptr, &logical_device));

  vkGetDeviceQueue(logical_device, queues.graphics_family_index, 0, &queues.graphics);
  vkGetDeviceQueue(logical_device, queues.present_family_index, 0, &queues.present);
}
