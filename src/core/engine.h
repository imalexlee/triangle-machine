#pragma once

#include "core/window.h"
#include "renderer/renderer.h"

class Engine {
public:
  Engine(){};
  ~Engine();

  void create();
  void start();

private:
  void destroy();
  Renderer _renderer;
  Window _window;
};
