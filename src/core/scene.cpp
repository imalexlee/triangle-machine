#include "scene.h"

#include "loaders/gltf_loader.h"
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <nlohmann/json.hpp>

void scene_load_gltf_paths(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths) {
    for (const auto& path : gltf_paths) {
        Entity new_entity = load_entity(backend, path);
        new_entity.id     = scene->entities.size() + 1;

        for (DrawObject& obj : new_entity.opaque_objs) {
            obj.mesh_data.entity_id = new_entity.id;
        }
        for (DrawObject& obj : new_entity.transparent_objs) {
            obj.mesh_data.entity_id = new_entity.id;
        }

        scene->entities.push_back(new_entity);
    }
}

void scene_load_gltf_path(Scene* scene, VkBackend* backend, const std::filesystem::path& gltf_path) {
    Entity new_entity = load_entity(backend, gltf_path);
    new_entity.id     = scene->entities.size() + 1;

    for (DrawObject& obj : new_entity.opaque_objs) {
        obj.mesh_data.entity_id = new_entity.id;
    }
    for (DrawObject& obj : new_entity.transparent_objs) {
        obj.mesh_data.entity_id = new_entity.id;
    }
    scene->entities.push_back(new_entity);
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

void scene_update(Scene* scene, VkBackend* backend) {
    if (!scene->update_requested || scene->selected_entity < 0) {
        return;
    }

    Entity* curr_entity = &scene->entities[scene->selected_entity];

    for (DrawObject& obj : curr_entity->opaque_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    for (DrawObject& obj : curr_entity->transparent_objs) {
        obj.mesh_data.global_transform = curr_entity->transform;
    }
    scene->velocity = glm::vec3(0);

    backend_update_accel_struct(backend, &curr_entity->transform, scene->selected_entity);

    scene->update_requested = false;
}

void scene_update_entity_pos(Scene* scene, uint16_t ent_id, const glm::vec3& new_pos) {
    if (ent_id == 0) {
        return;
    }
    Entity* entity = &scene->entities[ent_id - 1];

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), new_pos);

    for (DrawObject& obj : entity->opaque_objs) {
        obj.mesh_data.global_transform = entity->transform * translation;
    }
    for (DrawObject& obj : entity->transparent_objs) {
        obj.mesh_data.global_transform = entity->transform * translation;
    }
}

void scene_save(Scene* scene, const std::filesystem::path& path) {
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

    std::ifstream  file(path);
    nlohmann::json j = nlohmann::json::parse(file);

    nlohmann::json scene_object = j["scene"];

    nlohmann::json entity_arr = scene_object["entities"];

    std::vector<std::filesystem::path> gltf_paths;
    std::vector<glm::mat4>             transforms;

    gltf_paths.reserve(entity_arr.size());
    transforms.reserve(entity_arr.size());

    for (const auto& entity : entity_arr) {
        gltf_paths.push_back(entity["path"].get<std::string>());

        const auto& transform_arr = entity["transform"];

        glm::mat4 transform(1.0f);

        size_t idx = 0;
        for (const auto& value : transform_arr) {
            if (idx >= 16)
                break;

            size_t col          = idx / 4;
            size_t row          = idx % 4;
            transform[col][row] = value.get<float>();
            idx++;
        }

        transforms.push_back(transform);
    }

    scene_load_gltf_paths(scene, backend, gltf_paths);

    assert(gltf_paths.size() == transforms.size());
    for (size_t i = 0; i < scene->entities.size(); i++) {
        Entity* entity    = &scene->entities[i];
        entity->transform = transforms[i];
        // unused
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 skew;
        glm::vec4 perspective;

        glm::vec3 translation;
        glm::decompose(transforms[i], scale, rotation, translation, skew, perspective);

        entity->orig_pos = translation;

        scene->selected_entity = i;
        scene_request_update(scene);
        scene_update(scene, backend);
    }

    scene->selected_entity = -1;
}
