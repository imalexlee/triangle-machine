#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Vertex {
    glm::vec3 position;
    float     uv_x;
    glm::vec3 normal;
    float     uv_y;
    glm::vec4 color;
};

struct DrawObject {
    VkDescriptorSet mat_desc_set;
    VkDescriptorSet obj_desc_set;
    VkBuffer        index_buffer;
    uint32_t        indices_count;
    uint32_t        indices_start;
};

struct Entity {
    std::vector<DrawObject> transparent_objs;
    std::vector<DrawObject> opaque_objs;
    glm::vec3               pos;
};
