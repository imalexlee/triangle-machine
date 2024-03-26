#include "renderer.h"
#include <cassert>

Renderer* active_renderer = nullptr;

Renderer& get() { return *active_renderer; }

void Renderer::create(Window& window) {
  assert(active_renderer == nullptr);
  active_renderer = this;

  _vk_backend.create(window);
}

void Renderer::render() { _vk_backend.draw(); }

void Renderer::destroy() {}
