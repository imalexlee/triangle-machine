#include "engine.h"
#include <GLFW/glfw3.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

Engine::~Engine() { destroy(); };

void Engine::create() {
  _window.create(WIDTH, HEIGHT, "my engine!");
  _renderer.create(_window);
}

void Engine::start() {
  while (!glfwWindowShouldClose(_window.glfw_window)) {
    glfwPollEvents();
    _renderer.render();
  };
}

void Engine::destroy(){};
