#include "scene.h"

#include "loaders/gltf_loader.h"

#include <fmt/base.h>

void load_scene(Scene* scene, const VkBackend* backend, std::span<const char*> gltf_paths) {
    scene->entities.reserve(gltf_paths.size());
    for (const auto& path : gltf_paths) {
        scene->entities.push_back(load_entity(backend, path));
    }
}

void scene_key_callback(Scene* scene, int key, int action) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_UP) {
            scene->velocity.z = -scene->movement_speed;
        }
        if (key == GLFW_KEY_DOWN) {
            scene->velocity.z = scene->movement_speed;
        }
        if (key == GLFW_KEY_LEFT) {
            scene->velocity.x = -scene->movement_speed;
        }
        if (key == GLFW_KEY_RIGHT) {
            scene->velocity.x = scene->movement_speed;
        }
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_UP) {
            scene->velocity.z = 0;
        }
        if (key == GLFW_KEY_DOWN) {
            scene->velocity.z = 0;
        }
        if (key == GLFW_KEY_LEFT) {
            scene->velocity.x = 0;
        }
        if (key == GLFW_KEY_RIGHT) {
            scene->velocity.x = 0;
        }
    }
}

using namespace std::chrono;
static auto start_time = high_resolution_clock::now();

void update_scene(Scene* scene) {
    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    Entity* curr_entity = &scene->entities[scene->curr_entity_idx];
    curr_entity->pos += scene->velocity * time_elapsed;
    start_time = high_resolution_clock::now();
}
