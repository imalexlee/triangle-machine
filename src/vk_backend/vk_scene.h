#pragma once

#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_pipeline.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <vk_backend/vk_types.h>

/*
 * A scene in this renderer is heavily tied to the GLTF file format.
 * For that reason, most of these structures are a near 1:1 mapping of
 * the GLTF spec.
 */

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

struct DrawObjectPushConstants {
  glm::mat4 local_transform{1.f};
  VkDeviceAddress vertex_buffer_address;
};

struct MetallicRoughness {
  std::optional<AllocatedImage> color_texture;
  std::optional<AllocatedImage> metallic_rough_texture;
  uint32_t color_tex_coord{0};
  uint32_t metallic_rough_tex_coord{0};
  float metallic_factor;
  float roughness_factor;
  glm::vec4 color_factor;
};

enum AlphaMode { OPAQUE, MASK, BLEND };

struct Material {
  std::string name;
  MetallicRoughness metallic_roughness;
  fastgltf::AlphaMode alpha_mode;
};

struct Primitive {
  uint32_t indices_start;
  std::optional<std::shared_ptr<Material>> material;
  // references one of the pipeline info's in the overall scene
  // std::shared_ptr<PipelineInfo> pipeline_info;
};

struct MeshBuffers {
  AllocatedBuffer indices;
  AllocatedBuffer vertices;
  VkDeviceAddress vertex_buffer_address;
};

struct Mesh {
  std::string name;
  MeshBuffers buffers;
  std::vector<Primitive> primitives;
};

struct DrawNode {
  std::optional<std::shared_ptr<Mesh>> mesh;
  std::vector<DrawNode> children;
  glm::mat4 local_transform{1.f};
};

struct DrawContext {
  std::vector<std::reference_wrapper<DrawNode>> opaque_nodes;
};

class GLTFScene {
public:
  std::vector<std::shared_ptr<Mesh>> meshes;
  std::vector<std::shared_ptr<Material>> materials;
  std::shared_ptr<PipelineInfo> opaque_pipeline_info;
  std::vector<DrawNode> root_nodes;
  DrawContext draw_ctx;

  void update(uint32_t root_node_idx, glm::mat4 top_matrix = glm::mat4{1.f});

private:
  void add_nodes_to_context(DrawNode& root_node);
};
