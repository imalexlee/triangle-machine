#pragma once

#include "editor.h"

#include <span>
#include <vector>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_scene.h>

struct Scene {
    std::vector<Entity> entities;
    glm::vec3           velocity        = {0, 0, 0};
    uint32_t            curr_entity_idx = 0;
    float               movement_speed  = 5.f;
    bool                update_requested{false};
};

void scene_load(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths);

void scene_request_update(Scene* scene);

void scene_update(Scene* scene, const Editor* editor);

void scene_open(Scene* scene, VkBackend* backend, const std::filesystem::path& path);

void scene_key_callback(Scene* scene, int key, int action);
