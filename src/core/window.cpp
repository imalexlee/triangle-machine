#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "window.h"
#include <cstdlib>
#include <vk_backend/vk_debug.h>

Window::~Window() {
  DEBUG_PRINT("destroying GLFW window");
  glfwDestroyWindow(glfw_window);
  glfwTerminate();
};

void Window::create(uint32_t width, uint32_t height, const char* title) {
  this->width = width;
  this->height = height;
  _title = title;

  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  glfw_window = glfwCreateWindow(width, height, _title, nullptr, nullptr);
  if (!glfw_window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(glfw_window, key_callback);
}

VkSurfaceKHR Window::get_vulkan_surface(const VkInstance instance) {
  VkSurfaceKHR surface;
  VK_CHECK(glfwCreateWindowSurface(instance, glfw_window, nullptr, &surface));
  return surface;
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void Window::error_callback(int error, const char* description) { fmt::println("GLFW errow: {}", description); }
