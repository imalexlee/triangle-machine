#pragma once

#include "editor.h"

#include <span>
#include <vector>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_scene.h>

struct Scene {
    std::vector<Entity> entities{};
    glm::vec3           velocity{0, 0, 0};
    int                 selected_entity{-1};
    float               movement_speed{5.f};
    bool                update_requested{false};
};

void scene_load_gltf_paths(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths);

void scene_load_gltf_path(Scene* scene, VkBackend* backend, const std::filesystem::path& gltf_path);

void scene_request_update(Scene* scene);

void scene_update(Scene* scene, VkBackend* backend);

void scene_update_entity_pos(Scene* scene, uint16_t ent_id, const glm::vec3& offset);

void scene_open(Scene* scene, VkBackend* backend, const std::filesystem::path& path);

void scene_save(Scene* scene, const std::filesystem::path& path);

void scene_key_callback(Scene* scene, int key, int action);
