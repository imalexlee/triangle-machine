#include "scene.h"

#include "loaders/gltf_loader.h"
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <nlohmann/json.hpp>
#include <taskflow/taskflow/taskflow.hpp>

using namespace std::chrono;
void scene_load_gltf_paths(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths) {
    auto start_time = high_resolution_clock::now();
    for (const auto& path : gltf_paths) {
        auto start_load_time = high_resolution_clock::now();

        Entity new_entity = load_entity(backend, path);
        new_entity.id     = scene->entities.size() + 1;

        for (DrawObject& obj : new_entity.opaque_objs) {
            obj.mesh_data.entity_id = new_entity.id;
        }
        for (DrawObject& obj : new_entity.transparent_objs) {
            obj.mesh_data.entity_id = new_entity.id;
        }

        scene->entities.push_back(new_entity);
        auto  time_dur_load     = duration_cast<duration<float>>(high_resolution_clock::now() - start_load_time);
        float time_elapsed_load = time_dur_load.count();

        std::cout << "entity " + new_entity.name << " load time: " << time_elapsed_load << std::endl;
    }
    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();
    std::cout << "total load time: " << time_elapsed << std::endl;
}

// void scene_load_gltf_paths(Scene* scene, VkBackend* backend, std::span<std::filesystem::path> gltf_paths) {
//     auto         start_time = high_resolution_clock::now();
//     tf::Executor executor;
//
//     std::vector<std::future<Entity>> entity_futures;
//     entity_futures.reserve(gltf_paths.size());
//
//     for (const auto& path : gltf_paths) {
//         entity_futures.push_back(executor.async([&]() {
//             Entity new_entity = load_entity(backend, path);
//             new_entity.id     = scene->entities.size() + 1;
//
//             for (DrawObject& obj : new_entity.opaque_objs) {
//                 obj.mesh_data.entity_id = new_entity.id;
//             }
//             for (DrawObject& obj : new_entity.transparent_objs) {
//                 obj.mesh_data.entity_id = new_entity.id;
//             }
//             return new_entity;
//         }));
//     }
//
//     executor.wait_for_all();
//
//     std::vector<Entity> new_entities;
//     new_entities.reserve(entity_futures.size());
//     for (auto& future : entity_futures) {
//         new_entities.push_back(future.get());
//     }
//     // since entities load async, sort them to ensure proper order
//     std::ranges::sort(new_entities, [&](const Entity& e1, const Entity& e2) { return e1.id < e2.id; });
//
//     for (const auto& entity : new_entities) {
//         scene->entities.push_back(entity);
//     }
//     auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
//     float time_elapsed  = time_duration.count();
//     std::cout << "total load time: " << time_elapsed << std::endl;
// }

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

static glm::mat4 global_translation(1.f);
static glm::mat4 global_rotation(1.f);
static glm::mat4 final_transform(1.f);

void scene_update(Scene* scene, VkBackend* backend) {
    if (!scene->update_requested || scene->selected_entity < 0) {
        return;
    }

    Entity* curr_entity = &scene->entities[scene->selected_entity];

    glm::vec3 entity_base_translation = curr_entity->transform[3];

    glm::mat4 base_translation_mat     = glm::translate(glm::mat4(1.f), entity_base_translation);
    glm::mat4 orig_pos_translation_mat = glm::translate(glm::mat4(1.f), curr_entity->orig_pos);

    // glm::vec3 scale;
    // glm::quat rotation;
    // glm::vec3 skew;
    // glm::vec4 perspective;
    // glm::vec3 translation;
    // glm::decompose(curr_entity->transform, scale, rotation, translation, skew, perspective);
    //
    // glm::mat4 scale_mat = glm::scale(glm::mat4(1.f), scale);
    //
    // // first translate to origin, compute rotations, move back to offset, scale it, and offset it to world position
    //
    // final_transform =
    //     global_translation * orig_pos_translation_mat * global_rotation * glm::toMat4(rotation) * scale_mat *
    //     glm::inverse(orig_pos_translation_mat);

    static bool first_run = true;
    if (first_run) {
        first_run       = false;
        final_transform = curr_entity->transform;
    }
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

void scene_update_entity_pos(Scene* scene, VkBackend* backend, uint16_t ent_id, const glm::vec3& offset) {
    if (ent_id == 0)
        return;

    glm::mat4 translation = glm::translate(glm::mat4(1.f), offset);

    global_translation = translation;

    Entity* entity = &scene->entities[ent_id - 1];

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::vec3 translation_comp;
    glm::decompose(scene->entities[ent_id - 1].transform, scale, rotation, translation_comp, skew, perspective);

    glm::mat4 scale_mat = glm::scale(glm::mat4(1.f), scale);

    // first translate to origin, compute rotations, move back to offset, scale it, and offset it to world position

    glm::mat4 orig_pos_translation_mat = glm::translate(glm::mat4(1.f), entity->orig_pos);

    final_transform =
        global_translation * orig_pos_translation_mat * global_rotation * glm::toMat4(rotation) * scale_mat * glm::inverse(orig_pos_translation_mat);

    entity->transform = translation * entity->transform;

    scene_request_update(scene);
    scene->selected_entity = ent_id - 1;
    scene_update(scene, backend);
}

void scene_update_entity_rotation(Scene* scene, VkBackend* backend, uint16_t ent_id, float rot_degrees) {
    if (ent_id == 0)
        return;

    glm::mat4 rotation = glm::rotate(glm::mat4(1.f), glm::radians(rot_degrees), glm::vec3(0, 1, 0));

    global_rotation = rotation;

    const Entity* entity = &scene->entities[ent_id - 1];

    glm::vec3 scale;
    glm::quat rotation_comp;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::vec3 translation_comp;
    glm::decompose(scene->entities[ent_id - 1].transform, scale, rotation_comp, translation_comp, skew, perspective);

    glm::mat4 scale_mat = glm::scale(glm::mat4(1.f), scale);

    // first translate to origin, compute rotations, move back to offset, scale it, and offset it to world position

    glm::mat4 orig_pos_translation_mat = glm::translate(glm::mat4(1.f), entity->orig_pos);

    final_transform = global_translation * orig_pos_translation_mat * global_rotation * glm::toMat4(rotation_comp) * scale_mat *
                      glm::inverse(orig_pos_translation_mat);

    scene_request_update(scene);
    scene->selected_entity = ent_id - 1;
    scene_update(scene, backend);
}

void scene_revert_entity_transformation(Scene* scene, VkBackend* backend, uint16_t ent_id) {
    const Entity* entity = &scene->entities[ent_id - 1];
    final_transform      = entity->transform;

    scene_request_update(scene);
    scene->selected_entity = ent_id - 1;
    scene_update(scene, backend);
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

        std::vector<float> orig_pos_array;
        orig_pos_array.reserve(3);
        const glm::vec3& orig_pos = scene->entities[i].orig_pos;
        for (size_t idx = 0; idx < 3; idx++) {
            orig_pos_array.push_back(orig_pos[idx]);
        }

        entity["orig_pos"]  = orig_pos_array;
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
     *                  "path":"//fda/fda/fdsa",
     *                  "transform": [0,1.0,1...],
     *                  "orig_pos": [1,2,3],
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
    std::vector<glm::vec3>             orig_positions;

    gltf_paths.reserve(entity_arr.size());
    transforms.reserve(entity_arr.size());
    orig_positions.reserve(entity_arr.size());

    for (const auto& entity : entity_arr) {
        gltf_paths.push_back(entity["path"].get<std::string>());

        const auto& orig_pos_arr = entity["orig_pos"];

        glm::vec3 orig_pos = {0, 0, 0};
        size_t    idx      = 0;
        for (const auto& value : orig_pos_arr) {
            orig_pos[idx] = value.get<float>();
            idx++;
        }

        orig_positions.push_back(orig_pos);

        const auto& transform_arr = entity["transform"];

        glm::mat4 transform(1.0f);

        idx = 0;
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

        entity->orig_pos = orig_positions[i];

        scene->selected_entity = i;
        scene_request_update(scene);
        scene_update(scene, backend);
    }

    scene->selected_entity = -1;
}
