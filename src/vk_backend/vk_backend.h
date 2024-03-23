#pragma once

#include "core/window.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_device.h"
#include <vector>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>

class VkBackend {
public:
  VkBackend(){};
  ~VkBackend() { cleanup(); };

  void init(Window& window);

private:
  VkInstance _instance;
  VkSurfaceKHR _surface;
  Debugger _debugger;
  DeviceContext _device_context;
  SwapchainContext _swapchain_context;

  // initializers
  void create_instance(GLFWwindow* window);

  void cleanup();
  std::vector<const char*> get_instance_extensions(GLFWwindow* window);
};
