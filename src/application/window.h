#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

class Window {
public:
  GLFWwindow* glfw_window{};

  Window(){};
  ~Window();

  void init(int width, int height, const char* title);
  VkSurfaceKHR get_surface(const VkInstance instance);

private:
  int _width;
  int _height;
  const char* _title;

  static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void error_callback(int error, const char* description);
};
