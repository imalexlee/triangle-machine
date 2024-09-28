#pragma once

#include <span>
#include <vector>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_scene.h>

struct Scene {
    std::vector<Entity> entities;
    uint32_t            curr_entity_idx = 0;
    float               movement_speed  = 5.f;
    glm::vec3           velocity        = {0, 0, 0};
};

void scene_load(Scene* scene, VkBackend* backend, std::span<const char*> gltf_paths);

void scene_update(Scene* scene);

void scene_key_callback(Scene* scene, int key, int action);
