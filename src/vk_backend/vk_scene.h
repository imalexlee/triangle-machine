#pragma once

#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_pipeline.h"
#include <cstdint>
#include <memory>
#include <optional>
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

struct MaterialUniformData {
  glm::vec4 color_factors;
};

struct MetallicRoughness {
  AllocatedImage color_texture;
  std::optional<AllocatedImage> metallic_rough_texture;
  std::optional<AllocatedBuffer> material_uniform_buffer;
  uint32_t color_tex_coord;
  uint32_t metallic_rough_tex_coord;
  float metallic_factor;
  float roughness_factor;
  glm::vec4 color_factors;
};

struct Material {
  std::string name;
  MetallicRoughness metallic_roughness;
  VkDescriptorSet desc_set;
  fastgltf::AlphaMode alpha_mode;
};

struct MeshBuffers {
  AllocatedBuffer indices;
  AllocatedBuffer vertices;
  VkDeviceAddress vertex_buffer_address;
};

struct Primitive {
  // std::shared_ptr<Material> material;
  VkDescriptorSet desc_set;
  //   descriptor set from material ^
  // std::shared_ptr<MeshBuffers> mesh_buffers;
  //  descriptor set from material ^
  DrawConstants draw_constants;
  VkBuffer index_buffer;
  uint32_t indices_count;
  uint32_t indices_start;
};

struct Mesh {
  std::vector<Primitive> primitives;
  std::shared_ptr<MeshBuffers> buffers;
};

struct SceneNode {
  std::vector<SceneNode> children;
  glm::mat4 local_transform{1.f};
};

struct DrawContext {
  PipelineInfo opaque_pipeline_info;
  std::vector<Primitive> opaque_primitives;
  PipelineInfo transparent_pipeline_info;
  std::vector<Primitive> transparent_primitives;
};

class GLTFScene {
public:
  std::vector<std::shared_ptr<MeshBuffers>> mesh_buffers;
  std::vector<std::shared_ptr<Material>> materials;
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
