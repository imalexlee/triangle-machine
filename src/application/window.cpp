#define GLFW_INCLUDE_VULKAN
#include "window.h"
#include "fmt/base.h"
#include "types.h"
#include <cstdlib>

Window::Window(int width, int height, const char* title) : _width{width}, _height{height}, _title{title} { init(); }
Window::~Window() {
  glfwTerminate();
  glfwDestroyWindow(window);
};

void Window::init() {

  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  glfwSetErrorCallback(error_callback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(_width, _height, _title, nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(window, key_callback);
}

VkSurfaceKHR Window::get_surface(const VkInstance instance) {
  VkSurfaceKHR surface;
  VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
  return surface;
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void Window::error_callback(int error, const char* description) { fmt::println("GLFW errow: {}", description); }
