#include "scene.h"

#include "loaders/gltf_loader.h"
#include "simdjson.h"
#include <fstream>
#include <nlohmann/json.hpp>

void scene_load(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths) {
    scene->entities.reserve(gltf_paths.size());
    for (const auto& path : gltf_paths) {
        scene->entities.push_back(load_entity(backend, path));
    }
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

void scene_update(Scene* scene, const Editor* editor) {
    if (!scene->update_requested || editor->selected_entity < 0) {
        start_time = high_resolution_clock::now();
        return;
    }

    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    Entity* curr_entity = &scene->entities[editor->selected_entity];

    // glm::mat4 translation = glm::translate(glm::mat4{1.f}, scene->velocity * time_elapsed);
    // curr_entity->transform *= translation;
    for (DrawObject& obj : curr_entity->opaque_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    for (DrawObject& obj : curr_entity->transparent_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    scene->velocity = glm::vec3(0);

    start_time = high_resolution_clock::now();
}

void scene_save(Scene* scene) {
    // TODO: Write data straight to json file, overwriting it.
    // TODO: using nlohmann json
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

    scene_load(scene, backend, gltf_paths);

    assert(scene->entities.size() == gltf_paths.size() == transforms.size());
    for (size_t i = 0; i < scene->entities.size(); i++) {
        Entity* entity    = &scene->entities[i];
        entity->transform = transforms[i];
    }
}
