#pragma once

#include <cstdint>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct DeviceQueues {
  VkQueue graphics;
  VkQueue present;

  uint32_t graphics_family_index;
  uint32_t present_family_index;
};

class Device {
public:
  VkDevice logical_device;
  VkPhysicalDevice physical_device;
  DeviceQueues queues;

  void create(VkInstance instance, VkSurfaceKHR surface);
  void destroy();

private:
  void create_physical_device(VkInstance instance);
  void get_queue_family_indices(VkSurfaceKHR surface);
  void create_logical_device();
};
