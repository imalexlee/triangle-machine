#include "scene.h"

#include "loaders/gltf_loader.h"

#include <fmt/base.h>

void scene_load(Scene* scene, VkBackend* backend, std::span<const char*> gltf_paths) {
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
        scene->update_requested = true;
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
        if (scene->velocity == glm::vec3(0)) {
            scene->update_requested = false;
        }
    }
}

using namespace std::chrono;
static auto start_time = high_resolution_clock::now();

void scene_update(Scene* scene, const Editor* editor) {
    if (!scene->update_requested) {
        start_time = high_resolution_clock::now();
        return;
    }

    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    Entity* curr_entity = &scene->entities[editor->selected_entity];
    curr_entity->pos += scene->velocity * time_elapsed;

    glm::mat4 translation = glm::translate(glm::mat4{1.f}, scene->velocity * time_elapsed);
    for (DrawObject& obj : curr_entity->opaque_objs) {
        obj.mesh_data.global_transform *= translation;
    }
    for (DrawObject& obj : curr_entity->transparent_objs) {
        obj.mesh_data.global_transform *= translation;
    }

    start_time = high_resolution_clock::now();
}
