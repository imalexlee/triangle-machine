#pragma once

#include "vk_backend/debug.h"
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

class VkBackend {
public:
  VkBackend(){};
  ~VkBackend() { cleanup(); };

  void init(GLFWwindow* window);

private:
  VkInstance _instance;
  VkPhysicalDevice _physical_device;
  VkDevice _device;
  VkSurfaceKHR _surface;
  Debugger _debugger;

  // initializers
  void create_instance(GLFWwindow* window);

  void cleanup();
  std::vector<const char*> get_instance_extensions(GLFWwindow* window);
};
