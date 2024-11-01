#pragma once
#include <glm/vec3.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

struct MeshData {
    glm::mat4       global_transform{1.f};
    glm::mat4       local_transform{1.f};
    VkDeviceAddress vertex_buffer_address{};
    uint32_t        mat_i{0};
};

struct DrawObject {
    MeshData mesh_data{};
    VkBuffer index_buffer;
    uint32_t indices_count{};
    uint32_t indices_start{};
};

struct Entity {
    std::vector<DrawObject> transparent_objs;
    std::vector<DrawObject> opaque_objs;
    glm::vec3               pos = {0, 0, 0};
};

struct EntityPushConstants {
    // glm::vec3 pos{};
    glm::mat4 global_transform{1.f};
};
