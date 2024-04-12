#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/vk_pipeline.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <vk_backend/vk_types.h>

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

struct Primitive {
  AllocatedBuffer indices;
  AllocatedBuffer vertices;
  VkDeviceAddress vertex_buffer_address;
  std::optional<uint32_t> material_idx;
  // references one of the pipeline info's in the overall scene
  std::shared_ptr<PipelineInfo> pipeline_info;
};

struct Mesh {
  std::vector<Primitive> primitives;
  std::string name;
};

class DrawNode {
public:
  std::shared_ptr<Mesh> mesh;
  std::vector<DrawNode> children;
  glm::mat4 local_transform{1.f};

private:
  DeletionQueue _deletion_queue;
};

struct DrawContext {
  std::vector<DrawNode> opaque_nodes;
};

class GLTFScene {
public:
  uint32_t node_count;
  std::vector<std::shared_ptr<Mesh>> meshes;
  std::shared_ptr<PipelineInfo> opaque_pipeline_info;
  std::vector<DrawNode> root_nodes;
  DrawContext draw_ctx;

  // optional top matrix applies a transform to all primitives
  void update(glm::mat4 top_matrix = glm::mat4{1.f});

private:
  void fill_context(DrawNode& root_node, glm::mat4 top_matrix);
};
