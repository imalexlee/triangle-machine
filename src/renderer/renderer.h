#pragma once

#include "core/window.h"
#include "vk_backend/vk_backend.h"
#include <renderer/types.h>

class Renderer {
public:
  Renderer(){};
  ~Renderer() { destroy(); };

  void create(Window& window);

  static Renderer& get();
  void render();

private:
  VkBackend _vk_backend;

  void destroy();
};
