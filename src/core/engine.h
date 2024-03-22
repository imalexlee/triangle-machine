#pragma once
#include "core/window.h"
#include "renderer/renderer.h"

class Engine {
public:
  Engine(){};
  ~Engine();

  void init();
  void start();

private:
  void cleanup();
  Renderer _renderer;
  Window _window;
};
