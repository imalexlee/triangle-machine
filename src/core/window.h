#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

class Window {
public:
  GLFWwindow *glfw_window{};

  void create(uint32_t width, uint32_t height, const char *title);
  void destroy();

  VkSurfaceKHR get_vulkan_surface(const VkInstance instance);

  void register_key_callback(std::function<void(int, int, int, int)> fn_ptr);
  void register_cursor_callback(std::function<void(double, double)> fn_ptr);

  static inline uint32_t width;
  static inline uint32_t height;
  static inline bool resized;

private:
  const char *_title;

  static inline std::vector<std::function<void(int, int, int, int)>> _key_callbacks;
  static inline std::vector<std::function<void(double, double)>> _cursor_callbacks;

  static void cursor_callback(GLFWwindow *window, double x_pos, double y_pos);
  static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
  static void error_callback(int error, const char *description);
  static void resize_callback(GLFWwindow *window, int new_width, int new_height);
};
