#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_scene.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
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

[[maybe_unused]] static constexpr std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> purple_checkerboard = []() {
  std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> result{};
  // fix endianness
  uint32_t black = __builtin_bswap32(0x000000FF);
  uint32_t magenta = __builtin_bswap32(0xFF00FFFF);

  for (uint32_t x = 0; x < CHECKER_WIDTH; x++) {
    for (uint32_t y = 0; y < CHECKER_WIDTH; y++) {
      result[y * CHECKER_WIDTH + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  return result;
}();

[[maybe_unused]] static constexpr std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> black_image = []() {
  std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> result{};
  uint32_t black = __builtin_bswap32(0x000000FF);
  for (uint32_t& el : result) {
    el = black;
  }
  return result;
}();

[[maybe_unused]] static constexpr std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> white_image = []() {
  std::array<uint32_t, CHECKER_WIDTH * CHECKER_WIDTH> result{};
  uint32_t black = __builtin_bswap32(0xFFFFFFFF);
  for (uint32_t& el : result) {
    el = black;
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

std::vector<std::shared_ptr<VkSampler>> get_samplers(VkDevice device, fastgltf::Asset& asset) {
  std::vector<std::shared_ptr<VkSampler>> samplers;
  for (auto& gltf_sampler : asset.samplers) {
    auto sampler = std::make_shared<VkSampler>();

    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    switch (gltf_sampler.magFilter.value()) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      sampler_ci.magFilter = VK_FILTER_NEAREST;
      break;
    default:
      sampler_ci.magFilter = VK_FILTER_LINEAR;
    }

    switch (gltf_sampler.minFilter.value()) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      sampler_ci.minFilter = VK_FILTER_NEAREST;
      break;
    default:
      sampler_ci.minFilter = VK_FILTER_LINEAR;
    }

    switch (gltf_sampler.wrapS) {
    case fastgltf::Wrap::ClampToEdge:
      sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      break;
    case fastgltf::Wrap::Repeat:
      sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      break;
    default:
      sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    switch (gltf_sampler.wrapT) {
    case fastgltf::Wrap::ClampToEdge:
      sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      break;
    case fastgltf::Wrap::Repeat:
      sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      break;
    default:
      sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    VK_CHECK(vkCreateSampler(device, &sampler_ci, nullptr, sampler.get()));
    samplers.push_back(sampler);
  }

  return samplers;
}

//
void apply_primitive_matrices(std::vector<Mesh>& meshes, fastgltf::Asset& asset, GLTFScene& scene,
                              uint32_t root_node_idx, glm::mat4 parent_matrix = glm::mat4{1.f}) {

  SceneNode root_node{};
  auto& root_gltf_node = asset.nodes[root_node_idx];
  root_node.local_transform = get_transform_matrix(root_gltf_node, parent_matrix);
  for (uint32_t child_node_idx : root_gltf_node.children) {
    apply_primitive_matrices(meshes, asset, scene, child_node_idx, root_node.local_transform);
  }

  if (root_gltf_node.meshIndex.has_value()) {
    for (Primitive& primitive : meshes[root_gltf_node.meshIndex.value()].primitives) {
      primitive.draw_constants.local_transform = root_node.local_transform;
    }
  }
}

// allocates and fills an AllocatedImage with the textures data
AllocatedImage generate_texture(VkBackend* backend, fastgltf::Asset& asset, fastgltf::Texture& gltf_texture) {
  auto& image = asset.images[gltf_texture.imageIndex.value()];
  int width, height, nr_channels;
  AllocatedImage new_texture;

  std::visit(fastgltf::visitor{
                 []([[maybe_unused]] auto& arg) {},
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
                                  []([[maybe_unused]] auto& arg) {},
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

  // SAMPLER DATA
  new_scene.samplers = get_samplers(backend->_device_context.logical_device, asset);

  // MATERIAL DATA
  // 1. create desc layout
  // 2. create pool from layout with [material_count] sets
  // 3. allocate and fill material desc sets for each material object
  DescriptorLayoutBuilder layout_builder;
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  new_scene.desc_set_layout = layout_builder.build(backend->_device_context.logical_device,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

  std::vector<PoolSizeRatio> pool_sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                           {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  new_scene.desc_allocator.create(backend->_device_context.logical_device, asset.materials.size(), pool_sizes);

  DescriptorWriter desc_writer;

  fmt::println("Loading materials...");
  for (auto& material : asset.materials) {
    auto new_material = std::make_shared<Material>();
    new_material->name = material.name.c_str();
    // DEBUG_PRINT("mat name: %s", new_material->name.c_str());
    new_material->alpha_mode = material.alphaMode;
    new_material->metallic_roughness.metallic_factor = material.pbrData.metallicFactor;
    new_material->metallic_roughness.roughness_factor = material.pbrData.roughnessFactor;
    new_material->metallic_roughness.color_factors = glm::make_vec4(material.pbrData.baseColorFactor.data());

    // allocate and fill a buffer to hold the base color factor
    new_material->metallic_roughness.material_uniform_buffer =
        create_buffer(sizeof(MaterialUniformData), backend->_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    auto* material_uniform_data =
        (MaterialUniformData*)new_material->metallic_roughness.material_uniform_buffer.value().info.pMappedData;
    material_uniform_data->color_factors = new_material->metallic_roughness.color_factors;

    // write filled buffer to binding 0 of the material descriptor set
    desc_writer.write_buffer(0, new_material->metallic_roughness.material_uniform_buffer.value().buffer,
                             sizeof(MaterialUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (material.pbrData.baseColorTexture.has_value()) {

      auto& color_texture = asset.textures[material.pbrData.baseColorTexture.value().textureIndex];

      new_material->metallic_roughness.color_tex_coord = material.pbrData.baseColorTexture.value().texCoordIndex;
      new_material->metallic_roughness.color_texture = generate_texture(backend, asset, color_texture);
      desc_writer.write_image(1, new_material->metallic_roughness.color_texture.image_view,
                              *new_scene.samplers[color_texture.samplerIndex.value()],
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    } else {
      // use default texture
      new_material->metallic_roughness.color_tex_coord = 0;
      new_material->metallic_roughness.color_texture = backend->upload_texture_image(
          (void*)white_image.data(), VK_IMAGE_USAGE_SAMPLED_BIT, CHECKER_WIDTH, CHECKER_WIDTH);
      desc_writer.write_image(1, new_material->metallic_roughness.color_texture.image_view,
                              backend->_default_nearest_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      auto& metallic_rough_texture = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex];
      new_material->metallic_roughness.metallic_rough_tex_coord =
          material.pbrData.metallicRoughnessTexture.value().texCoordIndex;
      new_material->metallic_roughness.metallic_rough_texture =
          generate_texture(backend, asset, metallic_rough_texture);
      // desc_writer.write_image(1, new_material->metallic_roughness.color_texture.image_view,
      //                         *new_scene.samplers[metallic_rough_texture.samplerIndex.value()],
      //                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      //                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    } else {
      // use default texture
      new_material->metallic_roughness.metallic_rough_tex_coord = 0;
      new_material->metallic_roughness.metallic_rough_texture = backend->upload_texture_image(
          (void*)purple_checkerboard.data(), VK_IMAGE_USAGE_SAMPLED_BIT, CHECKER_WIDTH, CHECKER_WIDTH);
    }
    new_material->desc_set =
        new_scene.desc_allocator.allocate(backend->_device_context.logical_device, new_scene.desc_set_layout);
    desc_writer.update_set(backend->_device_context.logical_device, new_material->desc_set);

    new_scene.materials.push_back(new_material);
  }
  fmt::println("Materials loaded");

  // MESH DATA
  // temporary containers
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<Primitive> new_primitives;
  std::vector<Mesh> meshes;

  for (auto& mesh : asset.meshes) {
    auto new_mesh_buffers = std::make_shared<MeshBuffers>();
    // new_mesh->name = mesh.name.c_str();

    vertices.clear();
    indices.clear();
    new_primitives.clear();

    for (auto& primitive : mesh.primitives) {

      Primitive new_primitive{};

      // VERTEX POSITION DATA
      uint32_t vertex_start_pos = vertices.size();
      {
        // gltf primitives guarantee POSITION attribute is present. no null checking
        auto* position_it = primitive.findAttribute("POSITION");
        fastgltf::Accessor& position_accessor = asset.accessors[position_it->second];
        assert(position_accessor.bufferViewIndex.has_value() && "gltf file did not have standard position data");
        vertices.resize(vertex_start_pos + position_accessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, position_accessor, [&](glm::vec3 pos, std::size_t idx) {
          vertices[idx + vertex_start_pos].position = pos;
          // set default color to white
          vertices[idx + vertex_start_pos].color = glm::vec4{1.f};
        });
      }

      // VERTEX NORMAL DATA
      {
        auto* normal_it = primitive.findAttribute("NORMAL");
        fastgltf::Accessor& normal_accessor = asset.accessors[normal_it->second];
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, normal_accessor, [&](glm::vec3 normal, std::size_t idx) { vertices[idx].normal = normal; });
      }

      // VERTEX INDEX DATA
      {
        assert(primitive.indicesAccessor.has_value() && "gltf file does not have indices accessor");
        auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
        assert(index_accessor.bufferViewIndex.has_value() && "gltf file did not have standard index data");

        new_primitive.indices_start = indices.size();
        indices.resize(new_primitive.indices_start + index_accessor.count);

        // index will be offset by vertex_start_pos since all primitives are interleaved into one overall mesh
        // buffer
        fastgltf::iterateAccessorWithIndex<uint32_t>(
            asset, index_accessor, [&](uint32_t index, std::size_t i) { indices[i + vertex_start_pos] = index; });
      }
      // VERTEX UV TEXTURE DATA
      {
        if (primitive.materialIndex.has_value()) {
          new_primitive.material = new_scene.materials[primitive.materialIndex.value()];
          // this is the N associated with TEXCOORD_N
          uint32_t uv_color_tex_idx = new_primitive.material->metallic_roughness.color_tex_coord;
          std::string uv_attribute_key = "TEXCOORD_";
          // 48 is ASCII for '0'
          uv_attribute_key.push_back(48 + uv_color_tex_idx);

          auto* color_uv_it = primitive.findAttribute(uv_attribute_key);
          fastgltf::Accessor& color_uv_accessor = asset.accessors[color_uv_it->second];
          fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, color_uv_accessor, [&](glm::vec2 uv, std::size_t idx) {
            vertices[idx + vertex_start_pos].uv_x = uv.x;
            vertices[idx + vertex_start_pos].uv_y = uv.y;
          });
        } else {
          new_primitive.material = new_scene.materials[0];
        }
      }
      // VERTEX COLOR DATA
      {
        auto colors = primitive.findAttribute("COLOR_0");
        if (colors != primitive.attributes.end()) {
          DEBUG_PRINT("FOUND VERTEX COLORS");
        }
      }
      new_primitives.push_back(new_primitive);
    }
    // upload the vertex and index buffers for this mesh to the scene then attach it all to
    // the primitives for drawing
    *new_mesh_buffers = backend->upload_mesh_buffers(indices, vertices);
    new_scene.mesh_buffers.push_back(new_mesh_buffers);

    Mesh new_mesh{};
    new_mesh.primitives = new_primitives;
    new_mesh.buffers = new_mesh_buffers;
    meshes.push_back(new_mesh);
  }

  // only deal with first scene in the gltf for now
  // generate the node tree to attach all transform matrices to each node
  for (uint32_t root_node_idx : asset.scenes[0].nodeIndices) {
    apply_primitive_matrices(meshes, asset, new_scene, root_node_idx);
  }

  for (auto& mesh : meshes) {
    for (auto& new_primitive : mesh.primitives) {
      new_primitive.mesh_buffers = mesh.buffers;
      // cache indices count
      new_primitive.indices_count = mesh.buffers->indices.info.size / sizeof(uint32_t);
      new_primitive.draw_constants.vertex_buffer_address =
          get_buffer_device_address(backend->_device_context.logical_device, mesh.buffers->vertices);
      if (new_primitive.material->alpha_mode == fastgltf::AlphaMode::Blend) {
        new_scene.draw_ctx.transparent_primitives.push_back(new_primitive);
      } else {
        new_scene.draw_ctx.opaque_primitives.push_back(new_primitive);
      }
    }

    auto& primitives = new_scene.draw_ctx.opaque_primitives;
    std::sort(primitives.begin(), primitives.end(), [&](const Primitive& primitive_a, const Primitive& primitive_b) {
      if (primitive_a.material == primitive_b.material) {
        return primitive_a.indices_start < primitive_b.indices_start;
      } else {
        return primitive_a.material < primitive_b.material;
      }
    });
  }

  return new_scene;
}

void destroy_scene(VkDevice device, VmaAllocator allocator, GLTFScene& scene) {
  DEBUG_PRINT("Destroying Scene");
  scene.desc_allocator.destroy_pools(device);

  for (auto& sampler : scene.samplers) {
    vkDestroySampler(device, *sampler, nullptr);
  }
  for (auto& mesh : scene.mesh_buffers) {
    destroy_buffer(allocator, mesh->indices);
    destroy_buffer(allocator, mesh->vertices);
  }
  for (auto& material : scene.materials) {
    destroy_image(device, allocator, material->metallic_roughness.color_texture);
    if (material->metallic_roughness.metallic_rough_texture.has_value()) {
      destroy_image(device, allocator, material->metallic_roughness.metallic_rough_texture.value());
    }
    if (material->metallic_roughness.material_uniform_buffer.has_value()) {

      destroy_buffer(allocator, material->metallic_roughness.material_uniform_buffer.value());
    }
  }

  vkDestroyDescriptorSetLayout(device, scene.desc_set_layout, nullptr);

  vkDestroyPipelineLayout(device, scene.draw_ctx.opaque_pipeline_info.pipeline_layout, nullptr);
  vkDestroyPipelineLayout(device, scene.draw_ctx.transparent_pipeline_info.pipeline_layout, nullptr);

  vkDestroyPipeline(device, scene.draw_ctx.opaque_pipeline_info.pipeline, nullptr);
  vkDestroyPipeline(device, scene.draw_ctx.transparent_pipeline_info.pipeline, nullptr);
}
