#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/vk_scene.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fmt/base.h>
#include <fmt/format.h>
#include <iostream>
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
#include <variant>
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

// recursively fills a vector of nodes based on GLTF node tree
void generate_nodes(std::vector<GLTFNode>& out_node_buf, fastgltf::Asset& asset, uint32_t root_node_idx,
                    glm::mat4 parent_matrix = glm::mat4{1.f}) {

  auto& root_gltf_node = asset.nodes[root_node_idx];

  glm::mat4 transform = get_transform_matrix(root_gltf_node, parent_matrix);
  for (uint32_t child_node_idx : root_gltf_node.children) {
    generate_nodes(out_node_buf, asset, child_node_idx, transform);
  }

  if (root_gltf_node.meshIndex.has_value()) {
    GLTFNode new_node;
    new_node.transform = transform;
    new_node.mesh_idx = root_gltf_node.meshIndex.value();
    out_node_buf.push_back(new_node);
  }
}

std::vector<VkSampler> get_samplers(VkDevice device, fastgltf::Asset& asset) {
  std::vector<VkSampler> samplers;
  samplers.reserve(asset.samplers.size());

  for (auto& gltf_sampler : asset.samplers) {
    VkSampler sampler;

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

    VK_CHECK(vkCreateSampler(device, &sampler_ci, nullptr, &sampler));
    samplers.push_back(sampler);
  }

  return samplers;
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

                   const std::string path(file_path.uri.path().begin(),
                                          file_path.uri.path().end()); // thanks C++.
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

Scene load_scene(VkBackend* backend, std::filesystem::path path) {

  constexpr auto supported_extensions =
      fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform |
      fastgltf::Extensions::KHR_materials_clearcoat | fastgltf::Extensions::KHR_materials_specular |
      fastgltf::Extensions::KHR_materials_transmission | fastgltf::Extensions::KHR_materials_variants;

  fastgltf::Parser parser(supported_extensions);
  fastgltf::GltfDataBuffer data;
  data.loadFromFile(path);

  constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;

  auto load = parser.loadGltf(&data, path.parent_path(), gltf_options);

  if (auto error = load.error(); error != fastgltf::Error::None) {
    fmt::println("ERROR LOADING GLTF");
    std::exit(1);
  }

  fastgltf::Asset asset;
  asset = std::move(load.get());

  Scene new_scene;

  new_scene.samplers = get_samplers(backend->_device_context.logical_device, asset);
  assert(new_scene.samplers.size() == asset.samplers.size() &&
         "amount of samplers in scene does not match the amount in the GLTF file.");

  // load textures
  std::vector<GLTFTexture> textures;
  textures.reserve(asset.textures.size());

  for (auto& texture : asset.textures) {
    GLTFTexture new_texture;
    new_texture.tex = generate_texture(backend, asset, texture);

    if (texture.samplerIndex.has_value()) {
      new_texture.sampler = new_scene.samplers[texture.samplerIndex.value()];
    } else {
      new_texture.sampler = backend->_default_linear_sampler;
    }
    textures.push_back(new_texture);
  }

  // load materials
  DescriptorLayoutBuilder layout_builder;
  // create descriptor pool for all material data
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  new_scene.mat_desc_set_layout = layout_builder.build(backend->_device_context.logical_device,
                                                       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

  std::vector<PoolSizeRatio> mat_pool_sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}};

  new_scene.mat_desc_allocator.create(backend->_device_context.logical_device, asset.materials.size(), mat_pool_sizes);

  std::vector<GLTFMaterial> materials;
  materials.reserve(asset.materials.size());
  new_scene.material_buffers.reserve(asset.materials.size());

  uint32_t default_mat_idx = 0;

  std::vector<bool> texture_used;

  /*
   * textures arent garunteed to be used in GLTF
   * save used texture indices to clean up unused images
   * at the end of scope
   */
  texture_used.resize(textures.size(), false);

  for (uint32_t i = 0; i < asset.materials.size(); i++) {
    // metadata
    GLTFMaterial new_mat;
    // meat and potatoes
    MaterialBuffers new_bufs;

    const auto& material = asset.materials[i];

    new_mat.alpha_mode = material.alphaMode;
    new_mat.pbr.metallic_factor = material.pbrData.metallicFactor;
    new_mat.pbr.roughness_factor = material.pbrData.roughnessFactor;
    new_mat.pbr.color_factors = glm::make_vec4(material.pbrData.baseColorFactor.data());

    VkSampler color_tex_sampler;
    VkSampler metal_tex_sampler;

    if (material.pbrData.baseColorTexture.has_value()) {
      // new_mat.pbr.color_tex = textures[material.pbrData.baseColorTexture->textureIndex];
      assert(material.pbrData.baseColorTexture.value().textureIndex < textures.size() &&
             "accessing invalid color texture");
      uint32_t tex_index = material.pbrData.baseColorTexture.value().textureIndex;
      const GLTFTexture& color_texture = textures[tex_index];

      texture_used[tex_index] = true;
      // fmt::println("accessing texture {}",
      // material.pbrData.baseColorTexture.value().textureIndex);

      new_mat.pbr.color_tex_coord = material.pbrData.baseColorTexture.value().texCoordIndex;
      new_bufs.color_tex = color_texture.tex;
      // fmt::println("attaching color tex {}", fmt::ptr(color_texture.tex.image));
      color_tex_sampler = color_texture.sampler;

    } else {

      default_mat_idx = i;
      new_bufs.color_tex = backend->_default_texture;
      color_tex_sampler = backend->_default_linear_sampler;
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      // new_mat.pbr.metal_rough_tex =
      // textures[material.pbrData.metallicRoughnessTexture->textureIndex];
      assert(material.pbrData.metallicRoughnessTexture.value().textureIndex < textures.size() &&
             "accessing invalid metal rough texture");

      uint32_t tex_index = material.pbrData.metallicRoughnessTexture.value().textureIndex;
      const GLTFTexture& metal_texture = textures[tex_index];

      texture_used[tex_index] = true;

      new_mat.pbr.metal_rough_tex_coord = material.pbrData.metallicRoughnessTexture.value().texCoordIndex;
      new_bufs.metal_rough_tex = metal_texture.tex;
      // fmt::println("attaching metal tex {}", fmt::ptr(metal_texture.tex.image));
      metal_tex_sampler = metal_texture.sampler;
      // fmt::println("accessing texture {}",
      // material.pbrData.metallicRoughnessTexture.value().textureIndex);
    } else {

      new_bufs.metal_rough_tex = backend->_default_texture;
      metal_tex_sampler = backend->_default_linear_sampler;
    }

    new_bufs.mat_desc_set =
        new_scene.mat_desc_allocator.allocate(backend->_device_context.logical_device, new_scene.mat_desc_set_layout);

    // 1. fill in the uniform material buffer
    new_bufs.mat_uniform_buffer =
        create_buffer(sizeof(MaterialUniformData), backend->_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    auto* material_uniform_data = (MaterialUniformData*)new_bufs.mat_uniform_buffer.info.pMappedData;

    material_uniform_data->color_factors = new_mat.pbr.color_factors;
    material_uniform_data->metallic_factor = new_mat.pbr.metallic_factor;
    material_uniform_data->roughness_factor = new_mat.pbr.roughness_factor;

    DescriptorWriter desc_writer;

    desc_writer.write_buffer(0, new_bufs.mat_uniform_buffer.buffer, sizeof(MaterialUniformData), 0,
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // 2. put the texture information into the descriptor set
    desc_writer.write_image(1, new_bufs.color_tex.image_view, color_tex_sampler,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    desc_writer.write_image(2, new_bufs.metal_rough_tex.image_view, metal_tex_sampler,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    desc_writer.update_set(backend->_device_context.logical_device, new_bufs.mat_desc_set);

    new_scene.material_buffers.push_back(new_bufs);
    materials.push_back(new_mat);
  }

  // free unused textures from vram
  for (uint32_t i = 0; i < texture_used.size(); i++) {
    if (!texture_used[i]) {
      destroy_image(backend->_device_context.logical_device, backend->_allocator, textures[i].tex);
    }
  }

  assert(new_scene.material_buffers.size() == materials.size() && "material metadata and buffers do not match in size");

  assert(new_scene.material_buffers.size() == asset.materials.size() &&
         "allocated material buffers do not match size of materials in GLTF file");

  assert(materials.size() == asset.materials.size() &&
         "temp material metadata does not match size of materials in GLTF file");

  // load meshes
  std::vector<GLTFMesh> meshes;
  meshes.reserve(asset.meshes.size());

  uint32_t primitive_count = 0;

  for (auto& mesh : asset.meshes) {
    GLTFMesh new_mesh;
    new_mesh.primitives.reserve(mesh.primitives.size());
    primitive_count += mesh.primitives.size();

    // creating one index and one vertex buffer per mesh. primitives are given
    // an offset and length into the index buffer.
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;

    for (auto&& primitive : mesh.primitives) {

      uint32_t material_idx = primitive.materialIndex.value_or(default_mat_idx);

      auto& pos_accessor = asset.accessors[primitive.findAttribute("POSITION")->second];

      new_mesh.vertices.resize(vertex_count + pos_accessor.count);

      fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, pos_accessor, [&](glm::vec3 pos, std::size_t i) {
        new_mesh.vertices[i + vertex_count].position = pos;
        new_mesh.vertices[i + vertex_count].color = glm::vec4{1.f};
        // default uv coords to top left of texture image
        new_mesh.vertices[i + vertex_count].uv_x = 0.f;
        new_mesh.vertices[i + vertex_count].uv_y = 0.f;
      });

      auto normal_accessor = asset.accessors[primitive.findAttribute("NORMAL")->second];
      fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normal_accessor, [&](glm::vec3 normal, std::size_t i) {
        new_mesh.vertices[i + vertex_count].normal = normal;
      });

      // currently only loading in UV's for the color texture
      int color_tex_coord = materials[material_idx].pbr.color_tex_coord;
      std::string color_tex_coord_key = "TEXCOORD_";
      // 48 is ASCII for '0'. breaks if color_text_coord > 9 for now. shiver me timbers
      color_tex_coord_key.push_back(48 + color_tex_coord);

      auto* color_tex_attribute_it = primitive.findAttribute(color_tex_coord_key);
      if (color_tex_attribute_it != primitive.attributes.end()) {
        // found tex coord attribute
        auto& color_tex_accessor = asset.accessors[color_tex_attribute_it->second];
        fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, color_tex_accessor, [&](glm::vec2 uv, std::size_t i) {
          new_mesh.vertices[i + vertex_count].uv_x = uv.x;
          new_mesh.vertices[i + vertex_count].uv_y = uv.y;
        });
      }

      auto& idx_accessor = asset.accessors[primitive.indicesAccessor.value()];

      new_mesh.indices.resize(index_count + idx_accessor.count);

      fastgltf::iterateAccessorWithIndex<std::uint16_t>(asset, idx_accessor, [&](std::uint16_t vert_index, size_t i) {
        // index needs to be offset by vertex_count since we're combining all vertex data into
        // one buffer
        new_mesh.indices[i + index_count] = vert_index + vertex_count;
        assert(vert_index + vertex_count < new_mesh.vertices.size() &&
               "index is larger than the vertex buffer itself.");
      });

      GLTFPrimitive new_primitive;
      new_primitive.mat_idx = material_idx;
      new_primitive.indices_count = idx_accessor.count;
      new_primitive.indices_start = index_count;

      index_count += idx_accessor.count;
      vertex_count += pos_accessor.count;

      new_mesh.primitives.push_back(new_primitive);
    }
    assert(new_mesh.primitives.size() == mesh.primitives.size() &&
           "temp mesh and actual GLTF mesh do not have the same amount of primitives");

    meshes.push_back(new_mesh);
  }

  for (auto& mesh : meshes) {
    MeshBuffers new_mesh_bufs = backend->upload_mesh_buffers(mesh.indices, mesh.vertices);
    new_scene.mesh_buffers.push_back(new_mesh_bufs);

    for (auto& primitive : mesh.primitives) {

      primitive.mat_desc_set = new_scene.material_buffers[primitive.mat_idx].mat_desc_set;
      primitive.index_buffer = new_mesh_bufs.indices.buffer;
    }
  }

  layout_builder.clear();
  layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  new_scene.obj_desc_set_layout = layout_builder.build(backend->_device_context.logical_device,
                                                       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

  std::vector<PoolSizeRatio> obj_pool_sizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
  new_scene.obj_desc_allocator.create(backend->_device_context.logical_device, primitive_count, obj_pool_sizes);

  new_scene.draw_obj_uniform_buffers.reserve(primitive_count);
  new_scene.mesh_buffers.reserve(meshes.size());

  std::vector<DrawObject> opaque_draws;
  opaque_draws.reserve(primitive_count);
  std::vector<DrawObject> trans_draws;
  trans_draws.reserve(primitive_count);
  std::vector<GLTFNode> nodes;

  for (auto& scene : asset.scenes) {
    for (auto& root_node_idx : scene.nodeIndices) {
      generate_nodes(nodes, asset, root_node_idx);
    }
  }

  for (auto& node : nodes) {
    auto& mesh = meshes[node.mesh_idx];
    auto& mesh_buffers = new_scene.mesh_buffers[node.mesh_idx];
    for (auto& primitive : mesh.primitives) {

      AllocatedBuffer obj_uniform_buf;

      VkDescriptorSet draw_obj_desc_set =
          new_scene.obj_desc_allocator.allocate(backend->_device_context.logical_device, new_scene.obj_desc_set_layout);

      obj_uniform_buf =
          create_buffer(sizeof(DrawObjUniformData), backend->_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

      auto* obj_uniform_data = (DrawObjUniformData*)obj_uniform_buf.info.pMappedData;

      obj_uniform_data->local_transform = node.transform;
      obj_uniform_data->vertex_buffer_address =
          get_buffer_device_address(backend->_device_context.logical_device, mesh_buffers.vertices);

      DescriptorWriter desc_writer;

      desc_writer.write_buffer(0, obj_uniform_buf.buffer, sizeof(DrawObjUniformData), 0,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

      desc_writer.update_set(backend->_device_context.logical_device, draw_obj_desc_set);

      new_scene.draw_obj_uniform_buffers.push_back(obj_uniform_buf);

      primitive.obj_desc_set = draw_obj_desc_set;

      DrawObject new_draw_obj;
      new_draw_obj.index_buffer = mesh_buffers.indices.buffer;
      new_draw_obj.indices_count = primitive.indices_count;
      new_draw_obj.indices_start = primitive.indices_start;
      new_draw_obj.mat_desc_set = primitive.mat_desc_set;
      new_draw_obj.obj_desc_set = draw_obj_desc_set;

      fastgltf::AlphaMode alpha_mode = materials[primitive.mat_idx].alpha_mode;

      if (alpha_mode == fastgltf::AlphaMode::Blend) {
        trans_draws.push_back(new_draw_obj);
      } else {
        opaque_draws.push_back(new_draw_obj);
      }
    }
  }

  // sort opaque draws by material
  std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const DrawObject& obj_a, const DrawObject& obj_b) {
    if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
      return obj_a.indices_start < obj_b.indices_start;
    } else {
      return obj_a.mat_desc_set < obj_b.mat_desc_set;
    }
  });

  // sort transparent draws by material
  std::sort(trans_draws.begin(), trans_draws.end(), [&](const DrawObject& obj_a, const DrawObject& obj_b) {
    if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
      return obj_a.indices_start < obj_b.indices_start;
    } else {
      return obj_a.mat_desc_set < obj_b.mat_desc_set;
    }
  });

  new_scene.opaque_objs.resize(opaque_draws.size());
  new_scene.transparent_objs.resize(trans_draws.size());

  // copy sorted blocks of opaque/trans draws into final buffer
  memcpy(new_scene.opaque_objs.data(), opaque_draws.data(), opaque_draws.size() * sizeof(DrawObject));
  memcpy(new_scene.transparent_objs.data(), trans_draws.data(), trans_draws.size() * sizeof(DrawObject));

  return new_scene;
}

void destroy_scene(VkBackend* backend, Scene& scene) {

  DEBUG_PRINT("Destroying Scene");

  VkDevice device = backend->_device_context.logical_device;
  VmaAllocator allocator = backend->_allocator;

  scene.mat_desc_allocator.destroy_pools(device);
  scene.obj_desc_allocator.destroy_pools(device);

  for (auto& sampler : scene.samplers) {
    vkDestroySampler(device, sampler, nullptr);
  }
  for (auto& mesh : scene.mesh_buffers) {
    destroy_buffer(allocator, mesh.indices);
    destroy_buffer(allocator, mesh.vertices);
  }
  for (auto& material : scene.material_buffers) {

    if (material.color_tex.image != backend->_default_texture.image) {
      destroy_image(device, allocator, material.color_tex);
    }
    if (material.metal_rough_tex.image != backend->_default_texture.image) {
      destroy_image(device, allocator, material.metal_rough_tex);
    }
    destroy_buffer(allocator, material.mat_uniform_buffer);
  }

  for (auto& draw_obj_uniform : scene.draw_obj_uniform_buffers) {
    destroy_buffer(allocator, draw_obj_uniform);
  }

  vkDestroyDescriptorSetLayout(device, scene.mat_desc_set_layout, nullptr);
  vkDestroyDescriptorSetLayout(device, scene.obj_desc_set_layout, nullptr);

  vkDestroyPipelineLayout(device, scene.opaque_pipeline_info.pipeline_layout, nullptr);
  vkDestroyPipelineLayout(device, scene.transparent_pipeline_info.pipeline_layout, nullptr);

  vkDestroyPipeline(device, scene.opaque_pipeline_info.pipeline, nullptr);
  vkDestroyPipeline(device, scene.transparent_pipeline_info.pipeline, nullptr);
}
