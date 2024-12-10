#pragma once
#include "editor/editor.h"

#include <span>

struct MeshData {
    glm::mat4       global_transform{1.f};
    glm::mat4       local_transform{1.f};
    VkDeviceAddress vertex_buffer_address{};
    uint32_t        mat_i{0};
    uint32_t        entity_id{};
};

struct DrawObject {
    MeshData mesh_data{};
    VkBuffer index_buffer{};
    uint32_t indices_count{};
    uint32_t indices_start{};
};

struct Entity {
    std::vector<DrawObject> transparent_objs{};
    std::vector<DrawObject> opaque_objs{};
    std::filesystem::path   path{};
    std::string             name{};
    glm::mat4               transform{};
    glm::vec3               orig_pos{};
    uint32_t                id;
};

struct EntityPushConstants {
    // glm::vec3 pos{};
    glm::mat4 global_transform{1.f};
};
struct Scene {
    std::vector<Entity> entities{};
    glm::vec3           velocity{0, 0, 0};
    int                 selected_entity{-1};
    float               movement_speed{5.f};
    bool                update_requested{false};
};

void scene_load_gltf_paths(Scene* scene, Renderer* backend, std::span<std::filesystem::path> gltf_paths);

void scene_load_gltf_path(Scene* scene, Renderer* backend, const std::filesystem::path& gltf_path);

void scene_request_update(Scene* scene);

void scene_update(Scene* scene, Renderer* backend);

void scene_update_entity_pos(Scene* scene, Renderer* backend, uint16_t ent_id, const glm::vec3& offset);

void scene_revert_entity_transformation(Scene* scene, Renderer* backend, uint16_t ent_id);

void scene_update_entity_rotation(Scene* scene, Renderer* backend, uint16_t ent_id, float rot_degrees);

void scene_open(Scene* scene, Renderer* backend, const std::filesystem::path& path);

void scene_save(Scene* scene, const std::filesystem::path& path);
