#include "gltf_loader.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "global_utils.h"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_descriptor.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_scene.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fastgltf/glm_element_traits.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <string>
#include <variant>
#include <vk_backend/vk_sync.h>
#include <vulkan/vulkan_core.h>

struct DrawObjUniformData {
    glm::mat4       local_transform{1.f};
    VkDeviceAddress vertex_buffer_address;
};

// These are primarily temporary structures just for help
// with organizing loading GLTF files
struct GLTFTexture {
    AllocatedImage tex;
    VkSampler      sampler;
};

struct PBRMetallicRoughness {
    glm::vec4 color_factors;
    uint32_t  color_tex_coord{0};
    uint32_t  metal_rough_tex_coord{0};
    float     metallic_factor;
    float     roughness_factor;
};

struct GLTFMaterial {
    PBRMetallicRoughness pbr;
    fastgltf::AlphaMode  alpha_mode;
};

struct GLTFPrimitive {
    VkDescriptorSet     obj_desc_set;
    VkDescriptorSet     mat_desc_set;
    VkBuffer            index_buffer;
    uint32_t            indices_count;
    uint32_t            indices_start;
    uint32_t            mat_idx;
    fastgltf::AlphaMode alpha_mode;
};

struct GLTFMesh {
    std::vector<uint32_t>      indices;
    std::vector<Vertex>        vertices;
    glm::mat4                  local_transform{1.f};
    std::vector<GLTFPrimitive> primitives;
};

struct GLTFNode {
    glm::mat4 transform{1.f};
    uint32_t  mesh_idx;
};

struct MaterialUniformData {
    glm::vec4 color_factors;
    float     metallic_factor;
    float     roughness_factor;
};

struct MaterialBuffers {
    AllocatedBuffer mat_uniform_buffer;
    AllocatedImage  color_tex;
    AllocatedImage  metal_rough_tex;
    /*
     * this set contains bindings for the above resources
     * 0. mat_uniform_buffer
     * 1. color_tex
     * 2. metal_rough_tex
     */
    VkDescriptorSet mat_desc_set;
};

struct GLTFSceneResources {
    // currently only holding onto these to delete later
    std::vector<MeshBuffers>     mesh_buffers;
    std::vector<MaterialBuffers> material_buffers;
    std::vector<AllocatedBuffer> draw_obj_uniform_buffers;
    std::vector<VkSampler>       samplers;
};

glm::mat4 get_transform_matrix(const fastgltf::Node& node, glm::mat4x4& base) {
    if (const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
        return base * glm::mat4x4(glm::make_mat4x4(pMatrix->data()));
    }

    if (const auto* pTransform = std::get_if<fastgltf::TRS>(&node.transform)) {
        return base *
               glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data())) *
               glm::toMat4(glm::make_quat(pTransform->rotation.data())) *
               glm::scale(glm::mat4(1.0f), glm::make_vec3(pTransform->scale.data()));
    }

    return base;
}

AllocatedImage download_texture(const VkBackend*   backend,
                                fastgltf::Asset*   asset,
                                fastgltf::Texture* gltf_texture) {
    auto&          image = asset->images[gltf_texture->imageIndex.value()];
    int            width, height, nr_channels;
    AllocatedImage new_texture;

    std::visit(fastgltf::visitor{
                   []([[maybe_unused]] auto& arg) {

                   },
                   [&](fastgltf::sources::URI& file_path) {
                       assert(file_path.fileByteOffset == 0);
                       assert(file_path.uri.isLocalPath());

                       const std::string path(file_path.uri.path().begin(),
                                              file_path.uri.path().end()); // thanks C++.
                       uint8_t* data = stbi_load(path.c_str(), &width, &height, &nr_channels, 4);
                       new_texture =
                           upload_texture(backend, data, VK_IMAGE_USAGE_SAMPLED_BIT, height, width);
                       stbi_image_free(data);
                   },
                   [&](fastgltf::sources::Array& vector) {
                       uint8_t* data = stbi_load_from_memory(vector.bytes.data(),
                                                             static_cast<int>(vector.bytes.size()),
                                                             &width, &height, &nr_channels, 4);
                       new_texture =
                           upload_texture(backend, data, VK_IMAGE_USAGE_SAMPLED_BIT, height, width);
                       stbi_image_free(data);
                   },
                   [&](fastgltf::sources::BufferView& view) {
                       auto& buffer_view = asset->bufferViews[view.bufferViewIndex];
                       auto& buffer      = asset->buffers[buffer_view.bufferIndex];

                       std::visit(fastgltf::visitor{
                                      []([[maybe_unused]] auto& arg) {},
                                      [&](fastgltf::sources::Array& vector) {
                                          uint8_t* data = stbi_load_from_memory(
                                              vector.bytes.data() + buffer_view.byteOffset,

                                              static_cast<int>(buffer_view.byteLength), &width,
                                              &height, &nr_channels, 4);
                                          new_texture = upload_texture(backend, data,
                                                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                                                       height, width);
                                          stbi_image_free(data);
                                      }},
                                  buffer.data);
                   },
               },
               image.data);

    return new_texture;
};

// recursively fills a vector of nodes based on GLTF node tree
void generate_nodes(std::vector<GLTFNode>& out_node_buf,
                    fastgltf::Asset&       asset,
                    uint32_t               root_node_idx,
                    glm::mat4              parent_matrix = glm::mat4{1.f}) {

    auto& root_gltf_node = asset.nodes[root_node_idx];

    glm::mat4 transform = get_transform_matrix(root_gltf_node, parent_matrix);
    for (uint32_t child_node_idx : root_gltf_node.children) {
        generate_nodes(out_node_buf, asset, child_node_idx, transform);
    }

    if (root_gltf_node.meshIndex.has_value()) {
        GLTFNode new_node;
        new_node.transform = transform;
        new_node.mesh_idx  = root_gltf_node.meshIndex.value();
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

Entity load_scene(VkBackend* backend, std::filesystem::path path) {

    constexpr auto supported_extensions = fastgltf::Extensions::KHR_mesh_quantization |
                                          fastgltf::Extensions::KHR_texture_transform |
                                          fastgltf::Extensions::KHR_materials_clearcoat |
                                          fastgltf::Extensions::KHR_materials_specular |
                                          fastgltf::Extensions::KHR_materials_transmission |
                                          fastgltf::Extensions::KHR_materials_variants;

    fastgltf::Parser         parser(supported_extensions);
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);

    constexpr auto gltf_options =
        fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;

    auto load = parser.loadGltf(&data, path.parent_path(), gltf_options);

    if (auto error = load.error(); error != fastgltf::Error::None) {
        fmt::println("ERROR LOADING GLTF");
        std::exit(1);
    }

    fastgltf::Asset asset;
    asset = std::move(load.get());

    GLTFSceneResources new_scene;

    new_scene.samplers = get_samplers(backend->device_ctx.logical_device, asset);

    // load textures
    std::vector<GLTFTexture> textures;
    textures.reserve(asset.textures.size());

    for (auto& texture : asset.textures) {
        GLTFTexture new_texture;
        new_texture.tex = download_texture(backend, &asset, &texture);

        if (texture.samplerIndex.has_value()) {
            new_texture.sampler = new_scene.samplers[texture.samplerIndex.value()];
        } else {
            new_texture.sampler = backend->default_linear_sampler;
        }
        textures.push_back(new_texture);
    }

    std::vector<PoolSizeRatio> mat_pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
    };

    init_desc_allocator(&backend->mat_desc_allocator, backend->device_ctx.logical_device,
                        asset.materials.size(), mat_pool_sizes);

    std::vector<GLTFMaterial> materials;
    materials.reserve(asset.materials.size());
    new_scene.material_buffers.reserve(asset.materials.size());

    uint32_t default_mat_idx = 0;

    std::vector<bool> texture_used;

    /*
     * textures arent garunteed to be used in GLTF save used texture indices
     * to clean up unused image memory at the end of scope
     */
    texture_used.resize(textures.size(), false);

    for (uint32_t i = 0; i < asset.materials.size(); i++) {
        // metadata
        GLTFMaterial    new_mat;
        // meat and potatoes
        MaterialBuffers new_bufs;

        const auto& material = asset.materials[i];

        new_mat.alpha_mode           = material.alphaMode;
        new_mat.pbr.metallic_factor  = material.pbrData.metallicFactor;
        new_mat.pbr.roughness_factor = material.pbrData.roughnessFactor;
        new_mat.pbr.color_factors    = glm::make_vec4(material.pbrData.baseColorFactor.data());

        VkSampler color_tex_sampler;
        VkSampler metal_tex_sampler;

        if (material.pbrData.baseColorTexture.has_value()) {
            assert(material.pbrData.baseColorTexture.value().textureIndex < textures.size() &&
                   "accessing invalid color texture");
            uint32_t           tex_index = material.pbrData.baseColorTexture.value().textureIndex;
            const GLTFTexture& color_texture = textures[tex_index];

            texture_used[tex_index] = true;

            new_mat.pbr.color_tex_coord = material.pbrData.baseColorTexture.value().texCoordIndex;
            new_bufs.color_tex          = color_texture.tex;
            color_tex_sampler           = color_texture.sampler;

        } else {

            default_mat_idx    = i;
            new_bufs.color_tex = backend->default_texture;
            color_tex_sampler  = backend->default_linear_sampler;
        }

        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            uint32_t tex_index = material.pbrData.metallicRoughnessTexture.value().textureIndex;
            const GLTFTexture& metal_texture = textures[tex_index];

            texture_used[tex_index] = true;

            new_mat.pbr.metal_rough_tex_coord =
                material.pbrData.metallicRoughnessTexture.value().texCoordIndex;
            new_bufs.metal_rough_tex = metal_texture.tex;
            metal_tex_sampler        = metal_texture.sampler;
        } else {

            new_bufs.metal_rough_tex = backend->default_texture;
            metal_tex_sampler        = backend->default_linear_sampler;
        }

        new_bufs.mat_desc_set =
            allocate_desc_set(&backend->mat_desc_allocator, backend->device_ctx.logical_device,
                              backend->mat_desc_set_layout);

        // 1. fill in the uniform material buffer
        new_bufs.mat_uniform_buffer =
            create_buffer(sizeof(MaterialUniformData), backend->allocator,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT);

        auto* material_uniform_data =
            (MaterialUniformData*)new_bufs.mat_uniform_buffer.info.pMappedData;

        material_uniform_data->color_factors    = new_mat.pbr.color_factors;
        material_uniform_data->metallic_factor  = new_mat.pbr.metallic_factor;
        material_uniform_data->roughness_factor = new_mat.pbr.roughness_factor;

        DescriptorWriter desc_writer;

        write_buffer_desc(&desc_writer, 0, new_bufs.mat_uniform_buffer.buffer,
                          sizeof(MaterialUniformData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // 2. put the texture information into the descriptor set
        write_image_desc(&desc_writer, 1, new_bufs.color_tex.image_view, color_tex_sampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        write_image_desc(&desc_writer, 2, new_bufs.metal_rough_tex.image_view, metal_tex_sampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        update_desc_set(&desc_writer, backend->device_ctx.logical_device, new_bufs.mat_desc_set);

        new_scene.material_buffers.push_back(new_bufs);
        materials.push_back(new_mat);
    }

    // free unused textures memory
    for (uint32_t i = 0; i < texture_used.size(); i++) {
        if (!texture_used[i]) {
            destroy_image(backend->device_ctx.logical_device, backend->allocator, textures[i].tex);
        }
    }

    // load meshes
    std::vector<GLTFMesh> meshes;
    meshes.reserve(asset.meshes.size());

    uint32_t primitive_count = 0;

    for (auto& mesh : asset.meshes) {
        GLTFMesh new_mesh;
        new_mesh.primitives.reserve(mesh.primitives.size());
        primitive_count += mesh.primitives.size();

        // creating one index and one vertex buffer per mesh.primitives are given an
        // offset and length into the index buffer.
        uint32_t index_count  = 0;
        uint32_t vertex_count = 0;

        for (auto&& primitive : mesh.primitives) {

            uint32_t material_idx = primitive.materialIndex.value_or(default_mat_idx);

            auto& pos_accessor = asset.accessors[primitive.findAttribute("POSITION")->second];

            new_mesh.vertices.resize(vertex_count + pos_accessor.count);

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, pos_accessor, [&](glm::vec3 pos, std::size_t i) {
                    new_mesh.vertices[i + vertex_count].position = pos;
                    new_mesh.vertices[i + vertex_count].color    = glm::vec4{1.f};
                    // default uv
                    new_mesh.vertices[i + vertex_count].uv_x = 0.f;
                    new_mesh.vertices[i + vertex_count].uv_y = 0.f;
                });

            auto normal_accessor = asset.accessors[primitive.findAttribute("NORMAL")->second];
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, normal_accessor, [&](glm::vec3 normal, std::size_t i) {
                    new_mesh.vertices[i + vertex_count].normal = normal;
                });

            // currently only loading in UV's for the color texture
            int         color_tex_coord     = materials[material_idx].pbr.color_tex_coord;
            std::string color_tex_coord_key = "TEXCOORD_ ";
            // 48 is ASCII for '0'.
            // breaks if color_text_coord > 9 for now.
            // shiver me timbers
            color_tex_coord_key.push_back(48 + color_tex_coord);

            auto* color_tex_attribute_it = primitive.findAttribute(color_tex_coord_key);
            if (color_tex_attribute_it != primitive.attributes.end()) {
                // found tex coord attribute
                auto& color_tex_accessor = asset.accessors[color_tex_attribute_it->second];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(
                    asset, color_tex_accessor, [&](glm::vec2 uv, std::size_t i) {
                        new_mesh.vertices[i + vertex_count].uv_x = uv.x;
                        new_mesh.vertices[i + vertex_count].uv_y = uv.y;
                    });
            }

            auto& idx_accessor = asset.accessors[primitive.indicesAccessor.value()];

            new_mesh.indices.resize(index_count + idx_accessor.count);

            fastgltf::iterateAccessorWithIndex<std::uint16_t>(
                asset, idx_accessor, [&](std::uint16_t vert_index, size_t i) {
                    new_mesh.indices[i + index_count] = vert_index + vertex_count;
                });

            GLTFPrimitive new_primitive;
            new_primitive.mat_idx       = material_idx;
            new_primitive.indices_count = idx_accessor.count;
            new_primitive.indices_start = index_count;

            index_count += idx_accessor.count;
            vertex_count += pos_accessor.count;

            new_mesh.primitives.push_back(new_primitive);
        }
        meshes.push_back(new_mesh);
    }

    new_scene.mesh_buffers.reserve(meshes.size());
    for (auto& mesh : meshes) {
        MeshBuffers new_mesh_bufs = upload_mesh_buffers(backend, mesh.indices, mesh.vertices);
        new_scene.mesh_buffers.push_back(new_mesh_bufs);

        for (auto& primitive : mesh.primitives) {

            primitive.mat_desc_set = new_scene.material_buffers[primitive.mat_idx].mat_desc_set;
            primitive.index_buffer = new_mesh_bufs.indices.buffer;
        }
    }

    std::vector<PoolSizeRatio> obj_pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    };
    init_desc_allocator(&backend->obj_desc_allocator, backend->device_ctx.logical_device,
                        primitive_count, obj_pool_sizes);

    new_scene.draw_obj_uniform_buffers.reserve(primitive_count);
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
        auto& mesh         = meshes[node.mesh_idx];
        auto& mesh_buffers = new_scene.mesh_buffers[node.mesh_idx];
        for (auto& primitive : mesh.primitives) {

            AllocatedBuffer obj_uniform_buf;

            VkDescriptorSet draw_obj_desc_set =
                allocate_desc_set(&backend->obj_desc_allocator, backend->device_ctx.logical_device,
                                  backend->draw_obj_desc_set_layout);

            obj_uniform_buf = create_buffer(sizeof(DrawObjUniformData), backend->allocator,
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                VMA_ALLOCATION_CREATE_MAPPED_BIT);

            auto* obj_uniform_data = (DrawObjUniformData*)obj_uniform_buf.info.pMappedData;

            obj_uniform_data->local_transform       = node.transform;
            obj_uniform_data->vertex_buffer_address = get_buffer_device_address(
                backend->device_ctx.logical_device, mesh_buffers.vertices);

            DescriptorWriter desc_writer;

            write_buffer_desc(&desc_writer, 0, obj_uniform_buf.buffer, sizeof(DrawObjUniformData),
                              0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

            update_desc_set(&desc_writer, backend->device_ctx.logical_device, draw_obj_desc_set);

            new_scene.draw_obj_uniform_buffers.push_back(obj_uniform_buf);

            primitive.obj_desc_set = draw_obj_desc_set;

            DrawObject new_draw_obj;
            new_draw_obj.index_buffer  = mesh_buffers.indices.buffer;
            new_draw_obj.indices_count = primitive.indices_count;
            new_draw_obj.indices_start = primitive.indices_start;
            new_draw_obj.mat_desc_set  = primitive.mat_desc_set;
            new_draw_obj.obj_desc_set  = draw_obj_desc_set;

            fastgltf::AlphaMode alpha_mode = materials[primitive.mat_idx].alpha_mode;

            if (alpha_mode == fastgltf::AlphaMode::Blend) {
                trans_draws.push_back(new_draw_obj);
            } else {
                opaque_draws.push_back(new_draw_obj);
            }
        }
    }

    // sort by material
    std::sort(opaque_draws.begin(), opaque_draws.end(),
              [&](const DrawObject& obj_a, const DrawObject& obj_b) {
                  if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
                      return obj_a.indices_start < obj_b.indices_start;
                  } else {
                      return obj_a.mat_desc_set < obj_b.mat_desc_set;
                  }
              });

    std::sort(trans_draws.begin(), trans_draws.end(),
              [&](const DrawObject& obj_a, const DrawObject& obj_b) {
                  if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
                      return obj_a.indices_start < obj_b.indices_start;
                  } else {
                      return obj_a.mat_desc_set < obj_b.mat_desc_set;
                  }
              });

    Entity entity;
    entity.opaque_objs.resize(opaque_draws.size());
    entity.transparent_objs.resize(trans_draws.size());

    memcpy(entity.opaque_objs.data(), opaque_draws.data(),
           opaque_draws.size() * sizeof(DrawObject));
    memcpy(entity.transparent_objs.data(), trans_draws.data(),
           trans_draws.size() * sizeof(DrawObject));

    backend->deletion_queue.push_persistant([=]() {
        DEBUG_PRINT("Destroying Scene");

        VkDevice     device    = backend->device_ctx.logical_device;
        VmaAllocator allocator = backend->allocator;

        for (auto& sampler : new_scene.samplers) {
            vkDestroySampler(device, sampler, nullptr);
        }
        for (auto mesh : new_scene.mesh_buffers) {
            destroy_buffer(allocator, &mesh.indices);
            destroy_buffer(allocator, &mesh.vertices);
        }
        for (auto material : new_scene.material_buffers) {

            if (material.color_tex.image != backend->default_texture.image) {
                destroy_image(device, allocator, material.color_tex);
            }
            if (material.metal_rough_tex.image != backend->default_texture.image) {
                destroy_image(device, allocator, material.metal_rough_tex);
            }
            destroy_buffer(allocator, &material.mat_uniform_buffer);
        }

        for (auto draw_obj_uniform : new_scene.draw_obj_uniform_buffers) {
            destroy_buffer(allocator, &draw_obj_uniform);
        }
    });

    return entity;
}
