#pragma once
#include "vk_backend/vk_backend.h"
#include <renderer/types.h>

class Renderer {
public:
  Renderer(){};
  ~Renderer() { cleanup(); };

  void init(GLFWwindow* window);

  static Renderer& get();
  void run();

private:
  // Going to hook directly into vulkan since this is vulkan only.
  VkBackend _vk_backend{};

  void cleanup();
};
