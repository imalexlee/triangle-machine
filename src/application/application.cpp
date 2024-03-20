#include "application.h"
#include <GLFW/glfw3.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

void Application::start() {
  _window.init(WIDTH, HEIGHT, "my engine!");
  while (!glfwWindowShouldClose(_window.glfw_window)) {
    glfwPollEvents();
  };
}
