#include "vk_scene.h"

// update transform matrices of all nodes with a top matrix
void GLTFScene::update(glm::mat4 top_matrix) {
  for (DrawNode& node : draw_ctx.opaque_nodes) {
    node.local_transform *= top_matrix;
  }
}
