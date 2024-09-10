#include "gltf_loader.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "stb_image.h"

#include <fastgltf/glm_element_traits.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_scene.h>

// GLTF spec states clients should support at least 2 tex coordinates
static constexpr int tex_coord_num = 2;
struct Vertex {
    glm::vec4 position{};
    glm::vec4 normal{};
    glm::vec2 tex_coord[tex_coord_num] = {
        {0, 0},
        {0, 0}
    };
};

struct MeshData {
    glm::mat4       local_transform{1.f};
    VkDeviceAddress vertex_buffer_address{};
};

struct GLTFTexture {
    uint8_t*                data{};
    std::optional<uint32_t> sampler_i{};
    uint32_t                width{};
    uint32_t                height{};
    uint32_t                color_channels{};
};

struct TexCoordPair {
    std::optional<uint32_t> tex_i{};
    uint32_t                tex_coord_i{};
};

struct MaterialData {
    glm::vec4 color_factors{1.f, 1.f, 1.f, 1.f};
    float     metal_factor{};
    float     rough_factor{};
    uint32_t  color_tex_i{};
    uint32_t  metal_rough_tex_i{};
};

struct GLTFMaterial {
    TexCoordPair        base_color{};
    TexCoordPair        metal_rough{};
    MaterialData        mat_data{};
    fastgltf::AlphaMode alpha_mode = fastgltf::AlphaMode::Opaque;
};

struct GLTFPrimitive {
    fastgltf::PrimitiveType mode = fastgltf::PrimitiveType::TriangleStrip;
    std::optional<uint32_t> material_i{};
    uint32_t                indices_count{};
    uint32_t                indices_start{};
};

struct GLTFMesh {
    std::vector<GLTFPrimitive> primitives{};
    std::vector<uint32_t>      indices{};
    std::vector<Vertex>        vertices{};
};

struct GLTFNode {
    uint32_t    mesh_i{};
    glm::mat4x4 transform{1.f};
};

[[maybe_unused]] static const fastgltf::Sampler default_sampler = {
    .magFilter = fastgltf::Filter::Nearest,
    .minFilter = fastgltf::Filter::Nearest,
    .wrapS     = fastgltf::Wrap::Repeat,
    .wrapT     = fastgltf::Wrap::Repeat,
    .name      = "default_sampler",
};

constexpr TexCoordPair default_tex_coord_pair = {
    .tex_i       = 0,
    .tex_coord_i = 0,
};

[[maybe_unused]] static constexpr GLTFMaterial default_material = {
    .mat_data = {.color_factors = {1.f, 1.f, 1.f, 1.f}, .metal_factor = 1.0, .rough_factor = 1.0}
};

static std::vector<GLTFTexture> load_gltf_textures(const fastgltf::Asset* asset) {
    std::vector<GLTFTexture> gltf_textures{};
    gltf_textures.reserve(asset->textures.size());
    int width, height, nr_channels;

    for (const auto& texture : asset->textures) {
        GLTFTexture            new_tex{};
        const fastgltf::Image* image = &asset->images[texture.imageIndex.value()];
        new_tex.color_channels       = 4;
        std::visit(fastgltf::visitor{
                       []([[maybe_unused]] auto& arg) {},
                       [&](const fastgltf::sources::URI& file_path) {
                           assert(file_path.fileByteOffset == 0);
                           assert(file_path.uri.isLocalPath());

                           const std::string path(file_path.uri.path().begin(),
                                                  file_path.uri.path().end());

                           new_tex.data = stbi_load(path.c_str(), &width, &height, &nr_channels,
                                                    new_tex.color_channels);
                           assert(new_tex.data);

                           new_tex.width  = width;
                           new_tex.height = height;
                       },
                       [&](const fastgltf::sources::Array& vector) {
                           new_tex.data = stbi_load_from_memory(
                               vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width,
                               &height, &nr_channels, new_tex.color_channels);

                           assert(new_tex.data);
                           new_tex.width  = width;
                           new_tex.height = height;
                       },
                       [&](const fastgltf::sources::BufferView& view) {
                           auto& buffer_view = asset->bufferViews[view.bufferViewIndex];
                           auto& buffer      = asset->buffers[buffer_view.bufferIndex];

                           std::visit(fastgltf::visitor{
                                          []([[maybe_unused]] auto& arg) {},
                                          [&](const fastgltf::sources::Array& vector) {
                                              new_tex.data = stbi_load_from_memory(
                                                  vector.bytes.data() + buffer_view.byteOffset,
                                                  static_cast<int>(buffer_view.byteLength), &width,
                                                  &height, &nr_channels, new_tex.color_channels);
                                              assert(new_tex.data);

                                              new_tex.width  = width;
                                              new_tex.height = height;
                                          }},
                                      buffer.data);
                       },
                   },
                   image->data);

        if (texture.samplerIndex.has_value()) {
            new_tex.sampler_i = texture.samplerIndex.value();
        }

        gltf_textures.push_back(new_tex);
    }
    return gltf_textures;
}

std::vector<GLTFMaterial> load_gltf_materials(const fastgltf::Asset& my_asset) {
    std::vector<GLTFMaterial> gltf_materials{};
    gltf_materials.reserve(my_asset.materials.size());
    int i = 0;
    for (const auto& material : my_asset.materials) {
        i++;
        GLTFMaterial new_mat{};
        if (material.pbrData.baseColorTexture.has_value()) {
            const auto base_color_tex      = &material.pbrData.baseColorTexture.value();
            new_mat.base_color.tex_i       = base_color_tex->textureIndex;
            new_mat.base_color.tex_coord_i = base_color_tex->texCoordIndex;
            new_mat.mat_data.color_tex_i   = new_mat.base_color.tex_coord_i;
        }
        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            const auto metal_rough_tex         = &material.pbrData.metallicRoughnessTexture.value();
            new_mat.metal_rough.tex_i          = metal_rough_tex->textureIndex;
            new_mat.metal_rough.tex_coord_i    = metal_rough_tex->texCoordIndex;
            new_mat.mat_data.metal_rough_tex_i = new_mat.metal_rough.tex_coord_i;
        }
        new_mat.mat_data.metal_factor  = material.pbrData.metallicFactor;
        new_mat.mat_data.rough_factor  = material.pbrData.roughnessFactor;
        new_mat.mat_data.color_factors = glm::make_vec4(material.pbrData.baseColorFactor.data());
        new_mat.alpha_mode             = material.alphaMode;

        gltf_materials.push_back(new_mat);
    }

    return gltf_materials;
}

static std::vector<GLTFMesh> load_gltf_meshes(const fastgltf::Asset* my_asset) {
    std::vector<GLTFMesh> gltf_meshes{};
    gltf_meshes.reserve(my_asset->meshes.size());

    for (const auto& mesh : my_asset->meshes) {
        // tracks index offset for this primitive.
        // I will upload the indices and vertices buffers as two units for a mesh,
        // so primitives will index into their start position within the indices buffer
        uint32_t index_offset = 0;

        // additionally, I'll offset the actual vertex index in each element of the indices buffer
        // by how many elements are currently in the vertex buffer. This is for the same reason of
        // concatenating the vertex buffers of all primitives into one
        uint32_t vertex_count = 0;
        GLTFMesh new_mesh{};
        new_mesh.primitives.reserve(mesh.primitives.size());

        // first, iterate over primitives to find size necessary to allocate up front for buffers
        {
            uint32_t total_index_count  = 0;
            uint32_t total_vertex_count = 0;
            for (const auto& primitive : mesh.primitives) {
                assert(primitive.indicesAccessor.has_value());
                const fastgltf::Accessor* pos_accessor =
                    &my_asset->accessors[primitive.findAttribute("POSITION")->second];
                const fastgltf::Accessor* indices_accessor =
                    &my_asset->accessors[primitive.indicesAccessor.value()];

                total_index_count += indices_accessor->count;
                total_vertex_count += pos_accessor->count;
            }
            new_mesh.indices.reserve(total_index_count);
            // will resize instead of reserve for the vertices since we need to index
            // into it update in the next step
            new_mesh.vertices.resize(total_vertex_count);
        }

        for (const auto& primitive : mesh.primitives) {
            GLTFPrimitive new_primitive{};
            new_primitive.mode = primitive.type;

            if (primitive.materialIndex.has_value()) {
                new_primitive.material_i = primitive.materialIndex.value();
            }

            assert(primitive.findAttribute("NORMAL") != primitive.attributes.cend()); // no normal

            const fastgltf::Accessor* pos_accessor =
                &my_asset->accessors[primitive.findAttribute("POSITION")->second];
            const fastgltf::Accessor* indices_accessor =
                &my_asset->accessors[primitive.indicesAccessor.value()];
            const fastgltf::Accessor* normal_accessor =
                &my_asset->accessors[primitive.findAttribute("NORMAL")->second];

            new_primitive.indices_count = indices_accessor->count;
            new_primitive.indices_start = index_offset;
            new_mesh.primitives.push_back(new_primitive);

            fastgltf::iterateAccessor<uint32_t>(
                *my_asset, *indices_accessor, [&](uint32_t vert_index) {
                    new_mesh.indices.push_back(vert_index + vertex_count);
                });

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                *my_asset, *pos_accessor, [&](glm::vec3 pos, size_t i) {
                    new_mesh.vertices[i + vertex_count].position.x = pos.x;
                    new_mesh.vertices[i + vertex_count].position.y = pos.y;
                    new_mesh.vertices[i + vertex_count].position.z = pos.z;
                });

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                *my_asset, *normal_accessor, [&](glm::vec3 normal, size_t i) {
                    assert(i + vertex_count < new_mesh.vertices.size());
                    new_mesh.vertices[i + vertex_count].normal.x = normal.x;
                    new_mesh.vertices[i + vertex_count].normal.y = normal.y;
                    new_mesh.vertices[i + vertex_count].normal.z = normal.z;
                });
            for (int coord_i = 0; coord_i < tex_coord_num; coord_i++) {
                std::string coord_name          = "TEXCOORD_" + std::to_string(coord_i);
                const auto  tex_coord_attribute = primitive.findAttribute(coord_name);

                if (tex_coord_attribute != primitive.attributes.cend()) {
                    // found tex coord
                    const fastgltf::Accessor* tex_coord_accessor =
                        &my_asset->accessors[tex_coord_attribute->second];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        *my_asset, *tex_coord_accessor, [&](glm::vec2 tex_coord, size_t i) {
                            assert(i + vertex_count < new_mesh.vertices.size());
                            new_mesh.vertices[i + vertex_count].tex_coord[coord_i].x = tex_coord.x;
                            new_mesh.vertices[i + vertex_count].tex_coord[coord_i].y = tex_coord.y;
                        });
                } // else, tex coords default to 0 which is fine
            }
            vertex_count += pos_accessor->count;
            index_offset += indices_accessor->count;
        }
        gltf_meshes.push_back(new_mesh);
    }

    return gltf_meshes;
}

static std::vector<VkDescriptorSet>
create_mesh_desc_sets(VkBackend* backend, DescriptorAllocator* desc_allocator,
                      const VkDescriptorSetLayout set_layout, std::span<const GLTFNode> gltf_nodes,
                      const std::span<const MeshBuffers> vk_meshes) {
    std::vector<VkDescriptorSet> mesh_desc_sets;
    mesh_desc_sets.reserve(gltf_nodes.size());

    std::vector<PoolSizeRatio> mesh_pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    };
    init_desc_allocator(desc_allocator, backend->device_ctx.logical_device, gltf_nodes.size(),
                        mesh_pool_sizes);

    for (const auto& node : gltf_nodes) {
        VkDescriptorSet new_desc_set =
            allocate_desc_set(desc_allocator, backend->device_ctx.logical_device, set_layout);

        const AllocatedBuffer mesh_uniform_buffer =
            create_buffer(sizeof(MeshData), backend->allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT);

        auto* mapped_mesh_data = static_cast<MeshData*>(mesh_uniform_buffer.info.pMappedData);
        mapped_mesh_data->local_transform       = node.transform;
        mapped_mesh_data->vertex_buffer_address = get_buffer_device_address(
            backend->device_ctx.logical_device, &vk_meshes[node.mesh_i].vertices);

        DescriptorWriter desc_writer;
        write_buffer_desc(&desc_writer, 0, mesh_uniform_buffer.buffer, sizeof(MeshData), 0,
                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        update_desc_set(&desc_writer, backend->device_ctx.logical_device, new_desc_set);

        mesh_desc_sets.push_back(new_desc_set);

        backend->deletion_queue.push_persistant([=] {
            vmaDestroyBuffer(backend->allocator, mesh_uniform_buffer.buffer,
                             mesh_uniform_buffer.allocation);
        });
    }

    return mesh_desc_sets;
}
static int times_entered = 0;
// Adds draw objects to entity from a given root node
void       create_node_tree(const fastgltf::Asset* asset, const glm::mat4x4* transform,
                            const size_t node_i, std::vector<GLTFNode>* gltf_nodes) {

    times_entered++;
    const fastgltf::Node* node = &asset->nodes[node_i];
    GLTFNode              new_gltf_node{};
    if (const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&node->transform)) {
        new_gltf_node.transform = *transform * glm::mat4x4(glm::make_mat4x4(pMatrix->data()));
    } else if (const auto* pTransform = std::get_if<fastgltf::TRS>(&node->transform)) {
        new_gltf_node.transform =
            *transform *
            glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data())) *
            glm::toMat4(glm::make_quat(pTransform->rotation.data())) *
            glm::scale(glm::mat4(1.0f), glm::make_vec3(pTransform->scale.data()));
    } else {
        new_gltf_node.transform = *transform;
    }

    for (const size_t child_i : node->children) {
        create_node_tree(asset, &new_gltf_node.transform, child_i, gltf_nodes);
    }

    if (node->meshIndex.has_value()) {
        new_gltf_node.mesh_i = node->meshIndex.value();
        gltf_nodes->push_back(new_gltf_node);
    }
}

// turns nodes with a mesh into a flat buffer instead of GLTF's tree structure
static std::vector<GLTFNode> load_gltf_nodes(const fastgltf::Asset* asset) {
    std::vector<GLTFNode> gltf_nodes;
    gltf_nodes.reserve(asset->nodes.size());

    for (const auto& scene : asset->scenes) {
        for (const size_t root_node_i : scene.nodeIndices) {
            constexpr glm::mat4 base_transform = glm::mat4{1.f};
            create_node_tree(asset, &base_transform, root_node_i, &gltf_nodes);
        }
    }

    return gltf_nodes;
}

static std::vector<VkSampler> upload_gltf_samplers(const VkBackend*                   backend,
                                                   std::span<const fastgltf::Sampler> samplers) {
    std::vector<VkSampler> vk_samplers;
    vk_samplers.reserve(samplers.size());

    for (auto& gltf_sampler : samplers) {
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

        VK_CHECK(
            vkCreateSampler(backend->device_ctx.logical_device, &sampler_ci, nullptr, &sampler));
        vk_samplers.push_back(sampler);
    }

    return vk_samplers;
}

static std::vector<TextureSampler>
upload_gltf_textures(VkBackend* backend, std::span<const GLTFTexture> textures,
                     std::span<const fastgltf::Sampler> samplers) {
    const std::vector<VkSampler> vk_samplers = upload_gltf_samplers(backend, samplers);
    std::vector<TextureSampler>  tex_samplers;
    tex_samplers.reserve(tex_samplers.size());
    for (const auto& texture : textures) {
        TextureSampler new_tex_sampler{};
        new_tex_sampler.tex = upload_texture(backend, texture.data, VK_IMAGE_USAGE_SAMPLED_BIT,
                                             texture.color_channels, texture.width, texture.height);
        if (texture.sampler_i.has_value()) {
            new_tex_sampler.sampler = vk_samplers[texture.sampler_i.value()];
        } else {
            new_tex_sampler.sampler = backend->default_nearest_sampler;
        }
        tex_samplers.push_back(new_tex_sampler);
    }

    backend->deletion_queue.push_persistant([=] {
        for (const VkSampler sampler : vk_samplers) {
            vkDestroySampler(backend->device_ctx.logical_device, sampler, nullptr);
        }
        for (const auto& texture : tex_samplers) {
            vmaDestroyImage(backend->allocator, texture.tex.image, texture.tex.allocation);
            vkDestroyImageView(backend->device_ctx.logical_device, texture.tex.image_view, nullptr);
        }
    });
    return tex_samplers;
}

static std::vector<MeshBuffers> upload_gltf_mesh_buffers(VkBackend*                backend,
                                                         std::span<const GLTFMesh> meshes) {
    std::vector<MeshBuffers> mesh_buffers;
    mesh_buffers.reserve(meshes.size());
    for (const auto& mesh : meshes) {
        MeshBuffers new_mesh_buffers = upload_mesh<Vertex>(backend, mesh.indices, mesh.vertices);
        mesh_buffers.push_back(new_mesh_buffers);
    }

    backend->deletion_queue.push_persistant([=] {
        for (const auto& mesh_buffer : mesh_buffers) {
            vmaDestroyBuffer(backend->allocator, mesh_buffer.indices.buffer,
                             mesh_buffer.indices.allocation);
            vmaDestroyBuffer(backend->allocator, mesh_buffer.vertices.buffer,
                             mesh_buffer.vertices.allocation);
        }
    });
    return mesh_buffers;
}

// last element will be the default material desc set
static std::vector<VkDescriptorSet>
create_mat_desc_sets(VkBackend* backend, DescriptorAllocator* desc_allocator,
                     VkDescriptorSetLayout set_layout, std::span<const GLTFMaterial> gltf_materials,
                     std::span<const TextureSampler> vk_textures) {

    std::vector<VkDescriptorSet> mat_desc_sets;
    mat_desc_sets.reserve(gltf_materials.size() + 1);

    std::vector<PoolSizeRatio> mat_pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
    };
    init_desc_allocator(desc_allocator, backend->device_ctx.logical_device, gltf_materials.size(),
                        mat_pool_sizes);

    const TextureSampler default_tex_sampler = {
        .tex     = backend->default_texture,
        .sampler = backend->default_nearest_sampler,
    };

    for (const auto& material : gltf_materials) {
        // 1. allocate uninitialized desc set
        const VkDescriptorSet new_desc_set =
            allocate_desc_set(desc_allocator, backend->device_ctx.logical_device, set_layout);

        // 2. allocate gpu buffer for which we can write data to through a memory mapping
        // this is just a uniform buffer for some data about materials like color/metal factors
        const AllocatedBuffer mat_uniform_buffer =
            create_buffer(sizeof(MaterialData), backend->allocator,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT);

        auto* mapped_mat_data = static_cast<MaterialData*>(mat_uniform_buffer.info.pMappedData);
        mapped_mat_data->color_factors     = material.mat_data.color_factors;
        mapped_mat_data->metal_factor      = material.mat_data.metal_factor;
        mapped_mat_data->rough_factor      = material.mat_data.rough_factor;
        mapped_mat_data->color_tex_i       = material.mat_data.color_tex_i;
        mapped_mat_data->metal_rough_tex_i = material.mat_data.metal_rough_tex_i;

        // 3. assign the appropriate textures to this material or use default options
        const TextureSampler* color_tex_sampler       = &default_tex_sampler;
        const TextureSampler* metal_rough_tex_sampler = &default_tex_sampler;
        if (material.base_color.tex_i.has_value()) {
            color_tex_sampler = &vk_textures[material.base_color.tex_i.value()];
        }
        if (material.metal_rough.tex_i.has_value()) {
            metal_rough_tex_sampler = &vk_textures[material.metal_rough.tex_i.value()];
        }

        // 4. set descriptors in the desc set to point to all of our allocated and filled resources
        DescriptorWriter desc_writer;
        write_buffer_desc(&desc_writer, 0, mat_uniform_buffer.buffer, sizeof(MaterialData), 0,
                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        write_image_desc(&desc_writer, 1, color_tex_sampler->tex.image_view,
                         color_tex_sampler->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        write_image_desc(&desc_writer, 2, metal_rough_tex_sampler->tex.image_view,
                         metal_rough_tex_sampler->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        update_desc_set(&desc_writer, backend->device_ctx.logical_device, new_desc_set);

        mat_desc_sets.push_back(new_desc_set);

        backend->deletion_queue.push_persistant([=] {
            vmaDestroyBuffer(backend->allocator, mat_uniform_buffer.buffer,
                             mat_uniform_buffer.allocation);
        });
    }

    {
        // create final default mat desc set
        const VkDescriptorSet new_desc_set =
            allocate_desc_set(desc_allocator, backend->device_ctx.logical_device, set_layout);

        const AllocatedBuffer mat_uniform_buffer =
            create_buffer(sizeof(MaterialData), backend->allocator,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT);

        auto* mapped_mat_data = static_cast<MaterialData*>(mat_uniform_buffer.info.pMappedData);
        mapped_mat_data->color_factors = default_material.mat_data.color_factors;
        mapped_mat_data->metal_factor  = default_material.mat_data.metal_factor;
        mapped_mat_data->rough_factor  = default_material.mat_data.rough_factor;

        DescriptorWriter desc_writer;
        write_buffer_desc(&desc_writer, 0, mat_uniform_buffer.buffer, sizeof(MaterialData), 0,
                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        write_image_desc(&desc_writer, 1, default_tex_sampler.tex.image_view,
                         default_tex_sampler.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        write_image_desc(&desc_writer, 2, default_tex_sampler.tex.image_view,
                         default_tex_sampler.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        update_desc_set(&desc_writer, backend->device_ctx.logical_device, new_desc_set);

        mat_desc_sets.push_back(new_desc_set);

        backend->deletion_queue.push_persistant([=] {
            vmaDestroyBuffer(backend->allocator, mat_uniform_buffer.buffer,
                             mat_uniform_buffer.allocation);
        });
    }

    return mat_desc_sets;
}

Entity load_entity(VkBackend* backend, const std::filesystem::path& path) {

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

    const std::vector<GLTFTexture> gltf_textures  = load_gltf_textures(&asset);
    std::vector<GLTFMaterial>      gltf_materials = load_gltf_materials(asset);
    const std::vector<GLTFMesh>    gltf_meshes    = load_gltf_meshes(&asset);
    const std::vector<GLTFNode>    gltf_nodes     = load_gltf_nodes(&asset);

    const std::vector<TextureSampler> vk_textures =
        upload_gltf_textures(backend, gltf_textures, asset.samplers);
    const std::vector<MeshBuffers> vk_meshes = upload_gltf_mesh_buffers(backend, gltf_meshes);

    DescriptorAllocator mat_desc_allocator;
    DescriptorAllocator mesh_desc_allocator;

    DescriptorLayoutBuilder layout_builder;
    add_layout_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    add_layout_binding(&layout_builder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    add_layout_binding(&layout_builder, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    VkDescriptorSetLayout mat_desc_layout =
        build_set_layout(&layout_builder, backend->device_ctx.logical_device,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

    clear_layout_bindings(&layout_builder);

    add_layout_binding(&layout_builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    VkDescriptorSetLayout mesh_desc_layout =
        build_set_layout(&layout_builder, backend->device_ctx.logical_device,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

    const std::vector<VkDescriptorSet> vk_mat_desc_sets = create_mat_desc_sets(
        backend, &mat_desc_allocator, mat_desc_layout, gltf_materials, vk_textures);
    const std::vector<VkDescriptorSet> vk_mesh_desc_sets = create_mesh_desc_sets(
        backend, &mesh_desc_allocator, mesh_desc_layout, gltf_nodes, vk_meshes);

    Entity entity;
    for (size_t node_i = 0; node_i < gltf_nodes.size(); node_i++) {
        const GLTFNode* node = &gltf_nodes[node_i];
        const GLTFMesh* mesh = &gltf_meshes[node->mesh_i];
        for (const auto& primitive : mesh->primitives) {
            DrawObject new_draw_obj{};
            new_draw_obj.obj_desc_set  = vk_mesh_desc_sets[node_i];
            new_draw_obj.index_buffer  = vk_meshes[node->mesh_i].indices.buffer;
            new_draw_obj.indices_count = primitive.indices_count;
            new_draw_obj.indices_start = primitive.indices_start;

            // the last mat desc set will hold the default material
            const size_t mat_i        = primitive.material_i.value_or(vk_mat_desc_sets.size() - 1);
            new_draw_obj.mat_desc_set = vk_mat_desc_sets[mat_i];

            if (gltf_materials[mat_i].alpha_mode == fastgltf::AlphaMode::Blend) {
                entity.transparent_objs.push_back(new_draw_obj);
            } else {
                entity.opaque_objs.push_back(new_draw_obj);
            }
        }
    }

    // sort by material
    std::sort(entity.opaque_objs.begin(), entity.opaque_objs.end(),
              [&](const DrawObject& obj_a, const DrawObject& obj_b) {
                  if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
                      return obj_a.indices_start < obj_b.indices_start;
                  }
                  return obj_a.mat_desc_set < obj_b.mat_desc_set;
              });

    std::sort(entity.transparent_objs.begin(), entity.transparent_objs.end(),
              [&](const DrawObject& obj_a, const DrawObject& obj_b) {
                  if (obj_a.mat_desc_set == obj_b.mat_desc_set) {
                      return obj_a.indices_start < obj_b.indices_start;
                  }
                  return obj_a.mat_desc_set < obj_b.mat_desc_set;
              });

    entity.opaque_objs.shrink_to_fit();
    entity.transparent_objs.shrink_to_fit();

    for (auto& texture : gltf_textures) {
        stbi_image_free(texture.data);
    }

    backend->deletion_queue.push_persistant([=] {
        vkDestroyDescriptorSetLayout(backend->device_ctx.logical_device, mesh_desc_layout, nullptr);
        vkDestroyDescriptorSetLayout(backend->device_ctx.logical_device, mat_desc_layout, nullptr);

        for (const auto p : mesh_desc_allocator.ready_pools) {
            vkDestroyDescriptorPool(backend->device_ctx.logical_device, p, nullptr);
        }
        for (const auto p : mesh_desc_allocator.full_pools) {
            vkDestroyDescriptorPool(backend->device_ctx.logical_device, p, nullptr);
        }
        for (const auto p : mat_desc_allocator.ready_pools) {
            vkDestroyDescriptorPool(backend->device_ctx.logical_device, p, nullptr);
        }
        for (const auto p : mat_desc_allocator.full_pools) {
            vkDestroyDescriptorPool(backend->device_ctx.logical_device, p, nullptr);
        }
    });

    return entity;
}
