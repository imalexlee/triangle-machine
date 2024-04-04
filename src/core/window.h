#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>

class Window {
public:
  GLFWwindow* glfw_window{};

  Window(){};
  ~Window();

  void create(uint32_t width, uint32_t height, const char* title);
  VkSurfaceKHR get_vulkan_surface(const VkInstance instance);

  inline static uint32_t width;
  inline static uint32_t height;

  // if resize needed
  inline static bool resized;

private:
  const char* _title;

  static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void error_callback(int error, const char* description);
  static void resize_callback(GLFWwindow* window, int new_width, int new_height);
};
