#pragma once

#include "camera.h"
#include "core/window.h"
#include "vk_backend/vk_backend.h"
#include <core/ui.h>

struct Engine {
    VkBackend backend;
    Window    window;
    Camera    camera;
    UI        ui;
};

void init_engine(Engine* engine);

void run_engine(Engine* engine);

void deinit_engine(Engine* engine);
