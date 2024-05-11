#include "engine.h"
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
Engine *active_engine = nullptr;

void Engine::create() {
  assert(active_engine == nullptr);
  active_engine = this;

  glm::vec3 init_cam_pos = {0, -1, -8};

  _window.create(WIDTH, HEIGHT, "Triangle Machine");

  _camera.create(_window, init_cam_pos);

  _window.register_key_callback(_camera.key_callback);
  _window.register_cursor_callback(_camera.cursor_callback);

  _vk_backend.create(_window, _camera);
}

void Engine::run() {
  while (!glfwWindowShouldClose(_window.glfw_window)) {
    glfwPollEvents();

    if (_window.resized) {
      _vk_backend.resize();
      _window.resized = false;
      // continue;
    }

    _vk_backend.draw();
  };
}

void Engine::destroy(){};
