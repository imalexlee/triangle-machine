#pragma once

#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include <cstdint>
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// contains per-frame shader information
struct GlobalSceneData {
    glm::mat4 view_proj{1.f};
    glm::vec3 eye_pos;
};

struct DrawObjUniformData {
    glm::mat4 local_transform{1.f};
    VkDeviceAddress vertex_buffer_address;
};

struct GLTFTexture {
    AllocatedImage tex;
    VkSampler sampler;
};

struct PBRMetallicRoughness {
    glm::vec4 color_factors;
    uint32_t color_tex_coord{0};
    uint32_t metal_rough_tex_coord{0};
    float metallic_factor;
    float roughness_factor;
};

struct GLTFMaterial {
    PBRMetallicRoughness pbr;
    fastgltf::AlphaMode alpha_mode;
};

struct GLTFPrimitive {
    VkDescriptorSet obj_desc_set;
    VkDescriptorSet mat_desc_set;
    VkBuffer index_buffer;

    uint32_t indices_count;
    uint32_t indices_start;
    uint32_t mat_idx;
    fastgltf::AlphaMode alpha_mode;
};

struct GLTFMesh {
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    glm::mat4 local_transform{1.f};
    std::vector<GLTFPrimitive> primitives;
};

struct GLTFNode {
    glm::mat4 transform{1.f};
    uint32_t mesh_idx;
};

struct MaterialUniformData {
    glm::vec4 color_factors;
    float metallic_factor;
    float roughness_factor;
};

struct MaterialBuffers {
    AllocatedBuffer mat_uniform_buffer;
    AllocatedImage color_tex;
    AllocatedImage metal_rough_tex;
    /*
     * this set contains bindings..
     * 0. mat_uniform_buffer
     * 1. color_tex
     * 2. metal_rough_tex
     */
    VkDescriptorSet mat_desc_set;
};

struct DrawUniformData {
    glm::mat4 local_transform{1.f};
    glm::vec4 color_factors;
    VkDeviceAddress vertex_buffer_address;
};

struct MeshBuffers {
    AllocatedBuffer indices;
    AllocatedBuffer vertices;
};

struct DrawObject {
    VkDescriptorSet mat_desc_set;
    VkDescriptorSet obj_desc_set;
    VkBuffer index_buffer;
    uint32_t indices_count;
    uint32_t indices_start;
};

struct Scene {
    std::vector<MeshBuffers> mesh_buffers;
    std::vector<MaterialBuffers> material_buffers;
    std::vector<AllocatedBuffer> draw_obj_uniform_buffers;
    std::vector<DrawObject> transparent_objs;
    std::vector<DrawObject> opaque_objs;
    std::vector<VkSampler> samplers;

    DescriptorAllocator mat_desc_allocator;
    DescriptorAllocator obj_desc_allocator;
};
