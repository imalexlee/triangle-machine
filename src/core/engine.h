#pragma once

#include "audio.h"
#include "camera.h"
#include "scene.h"
#include "vk_backend/vk_backend.h"
#include <core/editor.h>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_scene.h>

enum class EngineMode {
    EDIT,
    RELEASE,
};

struct Engine {
    VkBackend backend{};
    Scene     scene{};
    Window    window{};
    Camera    camera{};
    Editor    editor{};

    AudioContext audio_ctx{};
    EngineMode   mode{};
};

void engine_init(Engine* engine, EngineMode mode);

void engine_run(Engine* engine);

void engine_deinit(Engine* engine);

// entity id at current pixel position. 0 = no id. other values are the index of an entity
// in the scene offset by 1. so entity id 5 would be index 4 in the entities array in the scene
[[nodiscard]] uint16_t engine_select_entity_at(const Engine* engine, int32_t x, int32_t y);
