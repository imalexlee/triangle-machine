#pragma once

#include "camera.h"
#include "scene.h"
#include "vk_backend/vk_backend.h"
#include <core/editor.h>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_scene.h>

struct Engine {
    VkBackend backend{};
    Scene     scene{};
    Window    window{};
    Camera    camera{};
    Editor    editor{};
};

void engine_init(Engine* engine);

void engine_run(Engine* engine);

void engine_deinit(Engine* engine);
