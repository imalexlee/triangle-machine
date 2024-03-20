#pragma once
#include "application/window.h"
#include "renderer/engine.h"

class Application {
public:
  Application(){};
  void start();
  void end();

private:
  Engine _engine;
  Window _window;
};
