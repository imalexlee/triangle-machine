#include "engine.h"
#include <GLFW/glfw3.h>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

Engine::~Engine() { cleanup(); };

void Engine::init() {
  _window.init(WIDTH, HEIGHT, "my engine!");
  _renderer.init(_window);
}

void Engine::start() {
  while (!glfwWindowShouldClose(_window.glfw_window)) {
    glfwPollEvents();
  };
}

void Engine::cleanup(){};
