#include "engine.h"

Engine* active_engine = nullptr;

Engine& get() { return *active_engine; }

void Engine::init(GLFWwindow* window) { _window = window; }

void Engine::run() {}
void Engine::cleanup() {}
