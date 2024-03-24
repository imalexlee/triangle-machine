#pragma once

#include "core/window.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_frame.h"
#include <cstdint>
#include <vector>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>

constexpr uint64_t FRAME_NUM = 3;

class VkBackend {
public:
  VkBackend(){};
  ~VkBackend() { destroy(); };

  void create(Window& window);
  void draw();

private:
  VkInstance _instance;
  VkSurfaceKHR _surface;
  Debugger _debugger;
  DeviceContext _device_context;
  SwapchainContext _swapchain_context;

  std::array<Frame, FRAME_NUM> _frames;
  uint64_t _frame_num{1};

  void create_instance(GLFWwindow* window);
  inline uint64_t get_frame_index() { return _frame_num % FRAME_NUM; }

  void destroy();
  std::vector<const char*> get_instance_extensions(GLFWwindow* window);
};
