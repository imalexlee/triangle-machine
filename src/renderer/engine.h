#pragma once
#include <renderer/types.h>

class Engine {
public:
  Engine(){};
  ~Engine() { cleanup(); };

  void init(GLFWwindow* window);

  static Engine& get();
  void run();

private:
  GLFWwindow* _window;

  void cleanup();
};
