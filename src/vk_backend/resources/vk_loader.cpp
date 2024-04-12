#include "vk_loader.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "fmt/base.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/vk_scene.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fastgltf/glm_element_traits.hpp>
#include <fstream>
#include <ios>
#include <memory>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

VkShaderModule load_shader_module(VkDevice device, const char* file_path) {

  std::ifstream file(file_path, std::ios::ate | std::ios::binary);
  size_t file_size = (size_t)file.tellg();
  std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

  file.seekg(0);
  file.read((char*)buffer.data(), file_size);
  file.close();

  VkShaderModule shader_module;
  VkShaderModuleCreateInfo shader_module_ci{};
  shader_module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_ci.codeSize = file_size;
  shader_module_ci.pCode = buffer.data();

  VK_CHECK(vkCreateShaderModule(device, &shader_module_ci, nullptr, &shader_module));

  return shader_module;
}

glm::mat4 get_transform_matrix(const fastgltf::Node& node, glm::mat4x4& base) {
  /** Both a matrix and TRS values are not allowed
   * to exist at the same time according to the spec */
  if (const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
    return base * glm::mat4x4(glm::make_mat4x4(pMatrix->data()));
  }

  if (const auto* pTransform = std::get_if<fastgltf::TRS>(&node.transform)) {
    return base * glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data())) *
           glm::toMat4(glm::make_quat(pTransform->rotation.data())) *
           glm::scale(glm::mat4(1.0f), glm::make_vec3(pTransform->scale.data()));
  }

  return base;
}

// returns a root node to a tree of nodes with appropriate data filled into them
// only adds nodes with a mesh to the draw context
DrawNode create_node_tree(fastgltf::Asset& asset, GLTFScene& scene, uint32_t root_node_idx) {
  DrawNode root_node;
  auto& root_gltf_node = asset.nodes[root_node_idx];
  glm::mat4 base_matrix = glm::mat4{1.f};
  root_node.local_transform = get_transform_matrix(root_gltf_node, base_matrix);
  for (uint32_t child_node_idx : root_gltf_node.children) {
    DrawNode new_child_node = create_node_tree(asset, scene, child_node_idx);
    root_node.children.push_back(new_child_node);
  }

  if (root_gltf_node.meshIndex.has_value()) {
    root_node.mesh = scene.meshes[root_gltf_node.meshIndex.value()];
    // maybe
    // try to get rid of the root_nodes tree if you can
    scene.draw_ctx.opaque_nodes.push_back(root_node);
  }

  return root_node;
}

GLTFScene load_scene(VkBackend* backend, std::filesystem::path path) {

  GLTFScene new_scene;
  fastgltf::Parser parser;
  fastgltf::GltfDataBuffer data;

  data.loadFromFile(path);

  fastgltf::Asset asset;
  auto load = parser.loadGltf(&data, path.parent_path());
  if (auto error = load.error(); error != fastgltf::Error::None) {
    fmt::println("ERROR LOADING GLTF");
    std::exit(1);
  }
  asset = std::move(load.get());

  // load vertex and index data first
  for (auto& mesh : asset.meshes) {
    auto new_mesh = std::make_shared<Mesh>();
    new_mesh->name = mesh.name.c_str();
    DEBUG_PRINT("MESH");

    for (auto& primitive : mesh.primitives) {
      Primitive new_primitive;

      // first get vertex positions
      // gltf primitives guarantee POSITION attribute is present. no null checking
      auto* position_it = primitive.findAttribute("POSITION");
      fastgltf::Accessor& position_accessor = asset.accessors[position_it->second];
      assert(position_accessor.bufferViewIndex.has_value() && "gltf file did not have standard position data");

      std::vector<Vertex> vertices(position_accessor.count);
      // pull out only the position information for now
      fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, position_accessor, [&](glm::vec3 pos, std::size_t idx) {
        vertices[idx].position = pos;
        // default to white
        vertices[idx].color = glm::vec4{1.f};
      });

      auto* normal_it = primitive.findAttribute("NORMAL");
      fastgltf::Accessor& normal_accessor = asset.accessors[normal_it->second];
      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset, normal_accessor, [&](glm::vec3 normal, std::size_t idx) { vertices[idx].normal = normal; });

      // then get index buffer
      assert(primitive.indicesAccessor.has_value() && "gltf file does not have indices accessor");
      auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
      assert(index_accessor.bufferViewIndex.has_value() && "gltf file did not have standard index data");

      std::vector<uint32_t> indices(index_accessor.count);
      fastgltf::iterateAccessorWithIndex<uint32_t>(asset, index_accessor,
                                                   [&](uint32_t index, std::size_t i) { indices[i] = index; });

      // allocate and fill buffers then get the address of the vertices
      Primitive uploaded_primitive = backend->upload_mesh_primitives(indices, vertices);
      uploaded_primitive.vertex_buffer_address =
          get_buffer_device_address(backend->_device_context.logical_device, uploaded_primitive.vertices);

      new_mesh->primitives.push_back(uploaded_primitive);
    }

    new_scene.meshes.push_back(new_mesh);
  }
  // only deal with first scene in the gltf for now
  std::vector<DrawNode> top_nodes(asset.scenes[0].nodeIndices.size());
  for (uint32_t root_node_idx : asset.scenes[0].nodeIndices) {
    DrawNode root_node = create_node_tree(asset, new_scene, root_node_idx);
    new_scene.root_nodes.push_back(root_node);
  }

  return new_scene;
}
