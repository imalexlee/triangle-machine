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

void load_scene(Scene* scene, VkBackend* backend, std::span<const char*> gltf_paths);

void update_scene(Scene* scene);

void scene_key_callback(Scene* scene, int key, int action);
