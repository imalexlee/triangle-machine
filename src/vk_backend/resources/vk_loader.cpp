#include "vk_loader.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include <fastgltf/glm_element_traits.hpp>
#include <fstream>
#include <vulkan/vulkan_core.h>

static int node_count = 0;

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
DrawNode create_node_tree(fastgltf::Asset& asset, GLTFScene& scene, uint32_t root_node_idx,
                          glm::mat4 parent_matrix = glm::mat4{1.f}) {

  DrawNode root_node;
  auto& root_gltf_node = asset.nodes[root_node_idx];
  root_node.local_transform = get_transform_matrix(root_gltf_node, parent_matrix);
  for (uint32_t child_node_idx : root_gltf_node.children) {
    DrawNode new_child_node = create_node_tree(asset, scene, child_node_idx, root_node.local_transform);
    root_node.children.push_back(new_child_node);
  }

  // don't include nodes with no mesh
  if (root_gltf_node.meshIndex.has_value()) {
    root_node.mesh = scene.meshes[root_gltf_node.meshIndex.value()];
  }
  node_count++;

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

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  // load vertex and index data first
  for (auto& mesh : asset.meshes) {
    auto new_mesh = std::make_shared<Mesh>();
    new_mesh->name = mesh.name.c_str();

    vertices.clear();
    indices.clear();

    for (auto& primitive : mesh.primitives) {
      Primitive new_primitive;

      // first get vertex positions
      // gltf primitives guarantee POSITION attribute is present. no null checking
      auto* position_it = primitive.findAttribute("POSITION");
      fastgltf::Accessor& position_accessor = asset.accessors[position_it->second];
      assert(position_accessor.bufferViewIndex.has_value() && "gltf file did not have standard position data");

      // reserve enough for how many we have and how many more we need
      uint32_t vertex_start_pos = vertices.size();
      vertices.resize(vertex_start_pos + position_accessor.count);

      // pull out only the position information for now
      fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, position_accessor, [&](glm::vec3 pos, std::size_t idx) {
        vertices[idx + vertex_start_pos].position = pos;
        // default to white
        vertices[idx + vertex_start_pos].color = glm::vec4{1.f};
      });

      auto* normal_it = primitive.findAttribute("NORMAL");
      fastgltf::Accessor& normal_accessor = asset.accessors[normal_it->second];
      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset, normal_accessor, [&](glm::vec3 normal, std::size_t idx) { vertices[idx].normal = normal; });

      // then get index data
      assert(primitive.indicesAccessor.has_value() && "gltf file does not have indices accessor");
      auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
      assert(index_accessor.bufferViewIndex.has_value() && "gltf file did not have standard index data");

      new_primitive.indices_start = indices.size();
      indices.resize(new_primitive.indices_start + index_accessor.count);

      // index will be offset by vertex_start_pos since all primitives are interleaved into one overall mesh buffer
      fastgltf::iterateAccessorWithIndex<uint32_t>(
          asset, index_accessor, [&](uint32_t index, std::size_t i) { indices[i + vertex_start_pos] = index; });

      new_mesh->primitives.push_back(new_primitive);
    }
    new_mesh->buffers = backend->upload_mesh_buffers(indices, vertices);
    new_mesh->buffers.vertex_buffer_address =
        get_buffer_device_address(backend->_device_context.logical_device, new_mesh->buffers.vertices);

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

void destroy_scene(VkDevice device, VmaAllocator allocator, GLTFScene& scene) {
  DEBUG_PRINT("Destroying Scene");
  for (auto& mesh : scene.meshes) {
    destroy_buffer(allocator, mesh->buffers.indices);
    destroy_buffer(allocator, mesh->buffers.vertices);
  }
  vkDestroyPipelineLayout(device, scene.opaque_pipeline_info->pipeline_layout, nullptr);
  vkDestroyPipeline(device, scene.opaque_pipeline_info->pipeline, nullptr);
}
