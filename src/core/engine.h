#pragma once

#include "core/window.h"
#include "vk_backend/vk_backend.h"

class Engine {
public:
  ~Engine() { destroy(); };

  void create();
  void run();

  static Engine& get();

private:
  void destroy();
  VkBackend _vk_backend;
  Window _window;
};
