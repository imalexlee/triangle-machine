#pragma once

#include "camera.h"
#include "core/window.h"
#include "vk_backend/vk_backend.h"
#include <core/ui.h>

class Engine {
  public:
    ~Engine() { destroy(); };

    void create();
    void run();

    static Engine& get();

  private:
    void      destroy();
    VkBackend _backend;
    Window    _window;
    Camera    _camera;
    UI        ui;
};
