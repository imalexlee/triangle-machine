#include "vk_scene.h"
#include <cstdint>
#include <global_utils.h>

// fills in the draw
// context with a
// specified node
// void
// GLTFScene::update_from_node(uint32_t
// root_node_idx,
// glm::mat4
// top_matrix) {
//  auto& root_node
//  =
//  root_nodes[root_node_idx];
//  add_nodes_to_context(root_node);
//  for (SceneNode&
//  node :
//  draw_ctx.nodes)
//  {
//    node.local_transform
//    *= top_matrix;
//  }
//}
//
// void
// GLTFScene::update_all_nodes(glm::mat4
// top_matrix) {
//  for (uint32_t i
//  = 0; i <
//  root_nodes.size();
//  i++) {
//    update_from_node(i,
//    top_matrix);
//  }
//}
//
// void
// GLTFScene::reset_draw_context()
// {
// draw_ctx.nodes.clear();
// }
//
// void
// GLTFScene::add_nodes_to_context(SceneNode&
// root_node) {
//
//  for (auto&
//  child_node :
//  root_node.children)
//  {
//    add_nodes_to_context(child_node);
//  }
//
//  if
//  (root_node.mesh.has_value())
//  {
//    draw_ctx.nodes.push_back(root_node);
//  }
//}
