#pragma once

#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_pipeline.h"
#include <cstdint>
#include <memory>
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

struct DrawConstants {
  glm::mat4 local_transform{1.f};
  VkDeviceAddress vertex_buffer_address;
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
  // GLTFTexture color_tex;
  // GLTFTexture metal_rough_tex;
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

  // try to get rid of this
  //  VkDeviceAddress vertex_buffer_address;
};

struct Primitive {
  // std::shared_ptr<Material> material;
  // contains DrawUniformData
  VkDescriptorSet desc_set;
  //   descriptor set from material ^
  // std::shared_ptr<MeshBuffers> mesh_buffers;
  //  descriptor set from material ^
  DrawConstants draw_constants;
  PipelineInfo pipeline_info;

  VkBuffer index_buffer;
  uint32_t indices_count;
  uint32_t indices_start;
};

struct Mesh {
  std::vector<Primitive> primitives;
  std::shared_ptr<MeshBuffers> buffers;
};

// struct GLTFNode {
//   std::vector<GLTFNode> children;
//   glm::mat4 local_transform{1.f};
// };

struct SceneNode {
  std::vector<SceneNode> children;
  glm::mat4 local_transform{1.f};
};

struct DrawContext {
  PipelineInfo opaque_pipeline_info;
  std::vector<Primitive> opaque_primitives;
  PipelineInfo transparent_pipeline_info;
  std::vector<Primitive> all_primitives;
  std::vector<Primitive> transparent_primitives;
};

class GLTFScene {
public:
  std::vector<std::shared_ptr<MeshBuffers>> mesh_buffers;
  std::vector<std::shared_ptr<GLTFMaterial>> materials;
  std::vector<std::shared_ptr<VkSampler>> samplers;
  DrawContext draw_ctx;

  VkDescriptorSetLayout desc_set_layout;
  DescriptorAllocator desc_allocator;

  void update_from_node(uint32_t root_node_idx, glm::mat4 top_matrix = glm::mat4{1.f});
  void update_all_nodes(glm::mat4 top_matrix = glm::mat4{1.f});
  void reset_draw_context();

private:
  void add_nodes_to_context(SceneNode& root_node);
};

struct DrawObject {
  VkDescriptorSet mat_desc_set;
  VkDescriptorSet obj_desc_set;
  VkBuffer index_buffer;
  uint32_t indices_count;
  uint32_t indices_start;
};

struct Scene {
public:
  std::vector<MeshBuffers> mesh_buffers;
  std::vector<MaterialBuffers> material_buffers;
  // GPU allocated buffers for per-draw_object data.
  // this uniform buffer is referenced by obj_desc_set in DrawObject
  std::vector<AllocatedBuffer> draw_obj_uniform_buffers;
  std::vector<DrawObject> draw_objects;
  std::vector<VkSampler> samplers;

  VkDescriptorSetLayout mat_desc_set_layout;
  VkDescriptorSetLayout obj_desc_set_layout;
  DescriptorAllocator mat_desc_allocator;
  DescriptorAllocator obj_desc_allocator;

  PipelineInfo opaque_pipeline_info;
  PipelineInfo transparent_pipeline_info;

  uint32_t trans_start;
};
