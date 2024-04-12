#include "vk_scene.h"

void GLTFScene::update(glm::mat4 top_matrix) {
  for (DrawNode& node : draw_ctx.opaque_nodes) {
    node.local_transform *= top_matrix;
  }
}

// update transform matrices of all nodes with a top matrix. fill in the draw context in the process
// void GLTFScene::fill_context(DrawNode& root_node, glm::mat4 top_matrix) {
//  root_node.local_transform *= top_matrix;
//  draw_ctx.opaque_surfaces.push_back()
//
//  for (DrawNode& child_node : root_node.children) {
//  }
//}
