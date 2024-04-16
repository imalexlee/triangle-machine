#include "vk_scene.h"
#include <cstdint>

// fills in the draw context with a specified node
void GLTFScene::update(uint32_t root_node_idx, glm::mat4 top_matrix) {
  draw_ctx.opaque_nodes.clear();
  auto& root_node = root_nodes[root_node_idx];
  add_nodes_to_context(root_node);
  for (DrawNode& node : draw_ctx.opaque_nodes) {
    node.local_transform *= top_matrix;
  }
}

void GLTFScene::set_pipelines(PipelineInfo opaque_pipeline) {
  // TODO: conditionally attach transparent pipelines too
}

void GLTFScene::add_nodes_to_context(DrawNode& root_node) {

  for (auto& child_node : root_node.children) {
    add_nodes_to_context(child_node);
  }

  if (root_node.mesh.has_value()) {
    draw_ctx.opaque_nodes.push_back(root_node);
  }
}
