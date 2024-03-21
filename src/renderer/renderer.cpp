#include "renderer.h"
#include <cassert>

Renderer* active_engine = nullptr;

Renderer& get() { return *active_engine; }

void Renderer::init(GLFWwindow* window) {
  assert(active_engine == nullptr);
  active_engine = this;

  _vk_backend.init(window);
}

void Renderer::run() {}
void Renderer::cleanup() {}
