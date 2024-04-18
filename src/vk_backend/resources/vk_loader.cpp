#include <cstdio>
#define STB_IMAGE_IMPLEMENTATION
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "global_utils.h"
#include "stb_image.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_loader.h"
#include <cstdint>
#include <fastgltf/glm_element_traits.hpp>
#include <fstream>
#include <memory>
#include <variant>
#include <vulkan/vulkan_core.h>

constexpr uint32_t CHECKER_WIDTH = 32;

static constexpr std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> purple_checkerboard = []() {
  // fix endianness
  std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> result{};
  uint32_t black = __builtin_bswap32(0x000000FF);
  uint32_t magenta = __builtin_bswap32(0xFF00FFFF);

  for (int x = 0; x < CHECKER_WIDTH; x++) {
    for (int y = 0; y < CHECKER_WIDTH; y++) {
      result[y * CHECKER_WIDTH + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  return result;
}();

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

  return root_node;
}

// allocates and fills an AllocatedImage with the textures data
AllocatedImage generate_texture(VkBackend* backend, fastgltf::Asset& asset, fastgltf::Texture& gltf_texture) {
  auto& image = asset.images[gltf_texture.imageIndex.value()];
  int width, height, nr_channels;
  AllocatedImage new_texture;

  std::visit(fastgltf::visitor{
                 [](auto& arg) {},
                 [&](fastgltf::sources::URI& file_path) {
                   assert(file_path.fileByteOffset == 0);
                   assert(file_path.uri.isLocalPath());

                   const std::string path(file_path.uri.path().begin(), file_path.uri.path().end()); // thanks C++.
                   uint8_t* data = stbi_load(path.c_str(), &width, &height, &nr_channels, 4);
                   new_texture = backend->upload_texture_image(data, VK_IMAGE_USAGE_SAMPLED_BIT, height, width);
                   stbi_image_free(data);
                 },
                 [&](fastgltf::sources::Array& vector) {
                   uint8_t* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                                                         &width, &height, &nr_channels, 4);
                   new_texture = backend->upload_texture_image(data, VK_IMAGE_USAGE_SAMPLED_BIT, height, width);
                   stbi_image_free(data);
                 },
                 [&](fastgltf::sources::BufferView& view) {
                   auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
                   auto& buffer = asset.buffers[buffer_view.bufferIndex];

                   std::visit(fastgltf::visitor{
                                  [](auto& arg) {},
                                  [&](fastgltf::sources::Array& vector) {
                                    uint8_t* data = stbi_load_from_memory(vector.bytes.data() + buffer_view.byteOffset,
                                                                          static_cast<int>(buffer_view.byteLength),
                                                                          &width, &height, &nr_channels, 4);
                                    new_texture =
                                        backend->upload_texture_image(data, VK_IMAGE_USAGE_SAMPLED_BIT, height, width);
                                    stbi_image_free(data);
                                  }},
                              buffer.data);
                 },
             },
             image.data);

  return new_texture;
};

GLTFScene load_scene(VkBackend* backend, std::filesystem::path path) {
  GLTFScene new_scene;

  static constexpr auto supported_extensions = fastgltf::Extensions::KHR_mesh_quantization |
                                               fastgltf::Extensions::KHR_texture_transform |
                                               fastgltf::Extensions::KHR_materials_variants;
  fastgltf::Parser parser(supported_extensions);

  fastgltf::GltfDataBuffer data;

  data.loadFromFile(path);

  fastgltf::Asset asset;
  constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;

  auto load = parser.loadGltf(&data, path.parent_path(), gltf_options);

  if (auto error = load.error(); error != fastgltf::Error::None) {
    fmt::println("ERROR LOADING GLTF");
    std::exit(1);
  }
  asset = std::move(load.get());

  // fill in material data
  fmt::println("Loading textures...");
  for (auto& material : asset.materials) {
    auto new_material = std::make_shared<Material>();
    new_material->name = material.name.c_str();
    new_material->alpha_mode = material.alphaMode;
    new_material->metallic_roughness.metallic_factor = material.pbrData.metallicFactor;
    new_material->metallic_roughness.roughness_factor = material.pbrData.roughnessFactor;
    new_material->metallic_roughness.color_factor = glm::make_vec4(material.pbrData.baseColorFactor.data());

    if (material.pbrData.baseColorTexture.has_value()) {
      auto& color_texture = asset.textures[material.pbrData.baseColorTexture.value().textureIndex];
      new_material->metallic_roughness.color_tex_coord = material.pbrData.baseColorTexture.value().texCoordIndex;
      new_material->metallic_roughness.color_texture = generate_texture(backend, asset, color_texture);
    } else {
      // use default texture
      new_material->metallic_roughness.color_tex_coord = 0;
      new_material->metallic_roughness.color_texture = backend->upload_texture_image(
          (void*)purple_checkerboard.data(), VK_IMAGE_USAGE_SAMPLED_BIT, CHECKER_WIDTH, CHECKER_WIDTH);
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      auto& metallic_rough_texture = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex];
      new_material->metallic_roughness.metallic_rough_tex_coord =
          material.pbrData.metallicRoughnessTexture.value().texCoordIndex;
      new_material->metallic_roughness.metallic_rough_texture =
          generate_texture(backend, asset, metallic_rough_texture);
    } else {
      // use default texture
      new_material->metallic_roughness.metallic_rough_tex_coord = 0;
      new_material->metallic_roughness.metallic_rough_texture = backend->upload_texture_image(
          (void*)purple_checkerboard.data(), VK_IMAGE_USAGE_SAMPLED_BIT, CHECKER_WIDTH, CHECKER_WIDTH);
    }
    new_scene.materials.push_back(new_material);
  }
  fmt::println("Textures loaded");

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
  for (auto& material : scene.materials) {
    if (material->metallic_roughness.color_texture.has_value()) {
      destroy_image(device, allocator, material->metallic_roughness.color_texture.value());
    }
    if (material->metallic_roughness.metallic_rough_texture.has_value()) {
      destroy_image(device, allocator, material->metallic_roughness.metallic_rough_texture.value());
    }
  }
  vkDestroyPipelineLayout(device, scene.opaque_pipeline_info->pipeline_layout, nullptr);
  vkDestroyPipeline(device, scene.opaque_pipeline_info->pipeline, nullptr);
}
