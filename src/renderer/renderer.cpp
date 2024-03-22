#include "renderer.h"
#include "core/window.h"
#include <cassert>

Renderer* active_renderer = nullptr;

Renderer& get() { return *active_renderer; }

void Renderer::init(Window& window) {
  assert(active_renderer == nullptr);
  active_renderer = this;

  _vk_backend.init(window);
}

void Renderer::run() {}
void Renderer::cleanup() {}
