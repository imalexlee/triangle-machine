#include "engine.h"
#include <GLFW/glfw3.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
Engine* active_engine = nullptr;

void Engine::create() {
  _window.create(WIDTH, HEIGHT, "my engine!");
  assert(active_engine == nullptr);
  active_engine = this;

  _vk_backend.create(_window);
}

void Engine::run() {
  while (!glfwWindowShouldClose(_window.glfw_window)) {
    glfwPollEvents();

    if (_window.resized) {
      _vk_backend.resize(_window.width, _window.height);
      _window.resized = false;
    }

    _vk_backend.draw();
  };
}

void Engine::destroy(){};
