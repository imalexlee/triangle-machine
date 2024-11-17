#include "scene.h"

#include "loaders/gltf_loader.h"
#include "simdjson.h"
#include <fstream>
#include <nlohmann/json.hpp>

void scene_load_gltf_paths(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths) {
    for (const auto& path : gltf_paths) {
        scene->entities.push_back(load_entity(backend, path));
    }
}

void scene_load_gltf_path(Scene* scene, VkBackend* backend, const std::string gltf_path) {
    scene->entities.push_back(load_entity(backend, gltf_path));
}

void scene_key_callback(Scene* scene, int key, int action) {
    /*if (action == GLFW_PRESS) {
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
    }*/
}

void scene_request_update(Scene* scene) { scene->update_requested = true; }

using namespace std::chrono;
static auto start_time = high_resolution_clock::now();

void scene_update(Scene* scene, VkBackend* backend) {
    if (!scene->update_requested || scene->selected_entity < 0) {
        start_time = high_resolution_clock::now();
        return;
    }

    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    Entity* curr_entity = &scene->entities[scene->selected_entity];

    // glm::mat4 translation = glm::translate(glm::mat4{1.f}, scene->velocity * time_elapsed);
    // curr_entity->transform *= translation;
    for (DrawObject& obj : curr_entity->opaque_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    for (DrawObject& obj : curr_entity->transparent_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    scene->velocity = glm::vec3(0);

    backend_update_accel_struct(backend, &curr_entity->transform, scene->selected_entity);

    start_time = high_resolution_clock::now();

    scene->update_requested = false;
}

void scene_append(Scene* scene, std::filesystem::path& entity_path) {}

void scene_save(Scene* scene, std::filesystem::path& path) {
    nlohmann::json output;

    nlohmann::json scene_obj;
    nlohmann::json entities = nlohmann::json::array();

    for (size_t i = 0; i < scene->entities.size(); i++) {
        nlohmann::json entity;

        entity["path"] = scene->entities[i].path.string();

        // Convert the transform matrix to array
        std::vector<float> transform_array;
        transform_array.reserve(16);

        const glm::mat4& transform = scene->entities[i].transform;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                transform_array.push_back(transform[col][row]);
            }
        }

        entity["transform"] = transform_array;
        entities.push_back(entity);
    }

    scene_obj["entities"] = entities;
    output["scene"]       = scene_obj;

    std::ofstream file(path);
    file << output.dump(4); // The 4 parameter adds pretty printing with indentation
    file.close();
}

void scene_open(Scene* scene, VkBackend* backend, const std::filesystem::path& path) {
    /*
     * {
     *    "scene": {
     *         "entities": [
     *             {
     *                  "path":"//fda/fda/fdsa"
     *                  "transform": [0,1.0,1...]
     *             },
     *         ],
     *    },
     * }
     */
    using namespace simdjson;
    ondemand::parser   parser;
    auto               json = padded_string::load(path.string());
    ondemand::document doc  = parser.iterate(json);

    ondemand::object scene_object = doc.get_object()["scene"];

    ondemand::array entity_arr = scene_object["entities"].get_array();

    std::vector<std::filesystem::path> gltf_paths;
    std::vector<glm::mat4>             transforms;

    gltf_paths.reserve(entity_arr.count_elements());
    transforms.reserve(entity_arr.count_elements());

    for (auto entity : entity_arr) {
        ondemand::object entity_obj = entity.get_object();

        // Get the path
        std::string_view new_path = entity_obj["path"].get_string();
        gltf_paths.push_back(new_path);

        // Get the transform array
        ondemand::array transform_arr = entity_obj["transform"].get_array();

        // Create a mat4 to store the transform
        glm::mat4 transform(1.0f); // Initialize with identity matrix

        // Counter for array index
        size_t idx = 0;

        // Iterate through the transform array and fill the matrix
        // glm::mat4 is column-major
        for (double value : transform_arr) {
            if (idx >= 16)
                break; // Ensure we don't exceed matrix bounds

            size_t col          = idx / 4;
            size_t row          = idx % 4;
            transform[col][row] = static_cast<float>(value);
            idx++;
        }

        transforms.push_back(transform);
    }

    scene_load_gltf_paths(scene, backend, gltf_paths);

    assert(gltf_paths.size() == transforms.size());
    for (size_t i = 0; i < scene->entities.size(); i++) {
        Entity* entity    = &scene->entities[i];
        entity->transform = transforms[i];

        // update all entities with their saved global transforms
        scene->selected_entity = i;
        scene_request_update(scene);
        scene_update(scene, backend);
    }

    scene->selected_entity = -1;
}
