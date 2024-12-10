#pragma once

#include "audio/audio.h"
#include "graphics/camera/camera.h"
#include "graphics/renderer/vk_renderer.h"
#include "scene/scene.h"
#include "system/device/vk_context.h"
#include <editor/editor.h>
#include <fastgltf/types.hpp>
#include <libshaderc_util/compiler.h>

enum class EngineMode {
    EDIT,
    RELEASE,
};

enum class EngineFeatures {
    NONE           = 0,
    DEBUG_GRID     = 1 << 1,
    SKY_BOX        = 1 << 2,
    ENTITY_PICKING = 1 << 3,
};

// in Windows environment, generates bitfield operators for an enum type
// todo: remove
DEFINE_ENUM_FLAG_OPERATORS(EngineFeatures)

struct Engine {
    VkContext    vk_ctx{};
    Renderer     backend{};
    Scene        scene{};
    Window       window{};
    Camera       camera{};
    Editor       editor{};
    AudioContext audio_ctx{};

    EngineMode     mode{};
    EngineFeatures features{EngineFeatures::NONE};
};

void engine_init(Engine* engine, EngineMode mode);

bool engine_is_alive(const Engine* engine);

void engine_begin_frame(const Engine* engine);

void engine_end_frame(Engine* engine);

void engine_deinit(Engine* engine);

// entity id at current pixel position. 0 = no id. other values are the index of an entity
// in the scene offset by 1. so entity id 5 would be index 4 in the entities array in the scene
[[nodiscard]] uint16_t engine_select_entity_at(const Engine* engine, int32_t x, int32_t y);

void engine_enable_features(Engine* engine, EngineFeatures features);

void engine_disable_features(Engine* engine, EngineFeatures features);
