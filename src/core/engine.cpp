#include "engine.h"
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;
Engine* active_engine = nullptr;

void Engine::create() {
  assert(active_engine == nullptr);
  active_engine = this;

  glm::vec3 init_cam_pos = {0, -1, 8}; // porsche, monkey
  glm::mat4 init_look_at = glm::lookAt(init_cam_pos, {0, 0, 0}, {0, 1, 0});

  _camera.create(init_cam_pos, init_look_at);

  _window.create(WIDTH, HEIGHT, "Triangle Machine");

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
