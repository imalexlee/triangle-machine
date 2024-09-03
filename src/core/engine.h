#pragma once

#include "camera.h"
#include "scene.h"
#include "vk_backend/vk_backend.h"
#include <core/ui.h>
#include <core/window.h>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_scene.h>

struct Engine {
    VkBackend backend;
    Scene     scene;
    Window    window;
    Camera    camera;
    // UI        ui;
};

void init_engine(Engine* engine);

void run_engine(Engine* engine);

void deinit_engine(Engine* engine);
