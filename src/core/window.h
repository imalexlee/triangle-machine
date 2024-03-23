#pragma once
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

class Window {
public:
  GLFWwindow* glfw_window{};

  Window(){};
  ~Window();

  void init(uint32_t width, uint32_t height, const char* title);
  VkSurfaceKHR get_surface(const VkInstance instance);

  uint32_t width;
  uint32_t height;

private:
  const char* _title;

  static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void error_callback(int error, const char* description);
};
