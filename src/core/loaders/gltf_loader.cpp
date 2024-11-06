#include "gltf_loader.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "stb_image.h"

#include <fastgltf/glm_element_traits.hpp>
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
struct GLTFImage {
    uint8_t* data{};
    uint32_t width{};
    uint32_t height{};
    uint32_t color_channels{};
};
struct GLTFTexture {
    std::optional<uint32_t> sampler_i{};
    uint32_t                image_i{};
};

struct TexCoordPair {
    std::optional<uint32_t> tex_i{};
    uint32_t                tex_coord_i{};
};

struct MaterialData {
    glm::vec4 color_factors{1.f, 1.f, 1.f, 1.f};
    float     metal_factor{0};
    float     rough_factor{0};
    int32_t   color_tex_i{-1}; // -1 represents no texture
    uint32_t  color_tex_coord{0};
    int32_t   metal_rough_tex_i{-1};
    uint32_t  metal_rough_tex_coord{0};
    float     padding[2];
};

struct GLTFMaterial {
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

    for (const auto& texture : asset->textures) {

        GLTFTexture new_texture{};
        if (texture.samplerIndex.has_value()) {
            new_texture.sampler_i = texture.samplerIndex.value();
        }
        new_texture.image_i = texture.imageIndex.value_or(0);

        gltf_textures.push_back(new_texture);
    }
    return gltf_textures;
}

static std::vector<GLTFImage> load_gltf_images(const fastgltf::Asset* asset) {
    std::vector<GLTFImage> gltf_images;
    gltf_images.reserve(asset->images.size());

    int width, height, channel_count;
    for (const auto& gltf_image : asset->images) {
        GLTFImage new_image{};

        std::visit(fastgltf::visitor{
                       []([[maybe_unused]] auto& arg) {},
                       [&](const fastgltf::sources::URI& file_path) {
                           assert(file_path.fileByteOffset == 0);
                           assert(file_path.uri.isLocalPath());

                           const std::string path(file_path.uri.path().begin(), file_path.uri.path().end());

                           new_image.data = stbi_load(path.c_str(), &width, &height, &channel_count, 4);
                           assert(new_image.data);
                       },
                       [&](const fastgltf::sources::Array& vector) {
                           new_image.data =
                               stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &channel_count, 4);
                           assert(new_image.data);
                       },
                       [&](const fastgltf::sources::BufferView& view) {
                           auto& buffer_view = asset->bufferViews[view.bufferViewIndex];
                           auto& buffer      = asset->buffers[buffer_view.bufferIndex];

                           std::visit(fastgltf::visitor{[]([[maybe_unused]] auto& arg) {},
                                                        [&](const fastgltf::sources::Array& vector) {
                                                            new_image.data = stbi_load_from_memory(vector.bytes.data() + buffer_view.byteOffset,
                                                                                                   static_cast<int>(buffer_view.byteLength), &width,
                                                                                                   &height, &channel_count, 4);
                                                            assert(new_image.data);
                                                        }},
                                      buffer.data);
                       },
                   },
                   gltf_image.data);

        new_image.width          = width;
        new_image.height         = height;
        new_image.color_channels = 4;

        gltf_images.push_back(new_image);
    }
    return gltf_images;
}

std::vector<GLTFMaterial> load_gltf_materials(const fastgltf::Asset* asset) {
    std::vector<GLTFMaterial> gltf_materials{};
    gltf_materials.reserve(asset->materials.size());
    for (const auto& material : asset->materials) {
        GLTFMaterial new_mat{};
        if (material.pbrData.baseColorTexture.has_value()) {
            const auto base_color_tex        = &material.pbrData.baseColorTexture.value();
            new_mat.mat_data.color_tex_coord = base_color_tex->texCoordIndex;
            new_mat.mat_data.color_tex_i     = base_color_tex->textureIndex;
        } else {
            new_mat.mat_data.color_tex_coord = 0;
        }
        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            const auto metal_rough_tex             = &material.pbrData.metallicRoughnessTexture.value();
            new_mat.mat_data.metal_rough_tex_coord = metal_rough_tex->texCoordIndex;
            new_mat.mat_data.metal_rough_tex_i     = metal_rough_tex->textureIndex;
        } else {
            new_mat.mat_data.metal_rough_tex_coord = 0;
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
                const fastgltf::Accessor* pos_accessor     = &my_asset->accessors[primitive.findAttribute("POSITION")->second];
                const fastgltf::Accessor* indices_accessor = &my_asset->accessors[primitive.indicesAccessor.value()];

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

            const fastgltf::Accessor* pos_accessor     = &my_asset->accessors[primitive.findAttribute("POSITION")->second];
            const fastgltf::Accessor* indices_accessor = &my_asset->accessors[primitive.indicesAccessor.value()];
            const fastgltf::Accessor* normal_accessor  = &my_asset->accessors[primitive.findAttribute("NORMAL")->second];

            new_primitive.indices_count = indices_accessor->count;
            new_primitive.indices_start = index_offset;
            new_mesh.primitives.push_back(new_primitive);

            fastgltf::iterateAccessor<uint32_t>(*my_asset, *indices_accessor,
                                                [&](uint32_t vert_index) { new_mesh.indices.push_back(vert_index + vertex_count); });

            fastgltf::iterateAccessorWithIndex<glm::vec3>(*my_asset, *pos_accessor, [&](glm::vec3 pos, size_t i) {
                new_mesh.vertices[i + vertex_count].position.x = pos.x;
                new_mesh.vertices[i + vertex_count].position.y = pos.y;
                new_mesh.vertices[i + vertex_count].position.z = pos.z;
            });

            fastgltf::iterateAccessorWithIndex<glm::vec3>(*my_asset, *normal_accessor, [&](glm::vec3 normal, size_t i) {
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
                    const fastgltf::Accessor* tex_coord_accessor = &my_asset->accessors[tex_coord_attribute->second];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(*my_asset, *tex_coord_accessor, [&](glm::vec2 tex_coord, size_t i) {
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

static std::vector<MeshData> create_mesh_data(const VkBackend* backend, std::span<const GLTFNode> gltf_nodes,
                                              const std::span<const MeshBuffers> vk_meshes) {
    std::vector<MeshData> mesh_data;
    mesh_data.reserve(gltf_nodes.size());
    for (const auto& node : gltf_nodes) {
        MeshData new_mesh_data;
        new_mesh_data.local_transform = node.transform;
        new_mesh_data.vertex_buffer_address =
            vk_buffer_device_address_get(backend->device_ctx.logical_device, vk_meshes[node.mesh_i].vertices.buffer);
        new_mesh_data.mat_i = 0; // default mat
        mesh_data.push_back(new_mesh_data);
    }

    return mesh_data;
}

static int times_entered = 0;
// Adds backend_draw objects to entity from a given root node
void create_node_tree(const fastgltf::Asset* asset, const glm::mat4x4* transform, const size_t node_i, std::vector<GLTFNode>* gltf_nodes) {

    times_entered++;
    const fastgltf::Node* node = &asset->nodes[node_i];
    GLTFNode              new_gltf_node{};
    if (const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&node->transform)) {
        new_gltf_node.transform = *transform * glm::mat4x4(glm::make_mat4x4(pMatrix->data()));
    } else if (const auto* pTransform = std::get_if<fastgltf::TRS>(&node->transform)) {
        new_gltf_node.transform = *transform * glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data())) *
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
static std::vector<GLTFNode> load_gltf_mesh_nodes(const fastgltf::Asset* asset) {
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

static std::vector<VkSampler> upload_gltf_samplers(VkBackend* backend, std::span<const fastgltf::Sampler> samplers) {
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

        VK_CHECK(vkCreateSampler(backend->device_ctx.logical_device, &sampler_ci, nullptr, &sampler));
        vk_samplers.push_back(sampler);

        backend->deletion_queue.push_persistent([=] {
            for (VkSampler vk_sampler : vk_samplers) {
                vkDestroySampler(backend->device_ctx.logical_device, vk_sampler, nullptr);
            }
        });
    }

    return vk_samplers;
}

static uint32_t upload_gltf_textures(VkBackend* backend, std::span<const GLTFImage> images, std::span<const GLTFTexture> textures,
                                     std::span<const fastgltf::Sampler> samplers) {
    const std::vector<VkSampler> vk_samplers = upload_gltf_samplers(backend, samplers);
    std::vector<TextureSampler>  tex_samplers;

    tex_samplers.reserve(textures.size());

    for (size_t i = 0; i < textures.size(); i++) {
        const GLTFTexture* texture = &textures[i];
        const GLTFImage*   image   = &images[texture->image_i];

        TextureSampler new_tex_sampler{};
        new_tex_sampler.view_type      = VK_IMAGE_VIEW_TYPE_2D;
        new_tex_sampler.width          = image->width;
        new_tex_sampler.height         = image->height;
        new_tex_sampler.layer_count    = 1;
        new_tex_sampler.color_channels = image->color_channels;
        new_tex_sampler.data           = image->data;

        if (texture->sampler_i.has_value()) {
            new_tex_sampler.sampler = vk_samplers[texture->sampler_i.value()];
        } else {
            new_tex_sampler.sampler = backend->default_nearest_sampler;
        }
        tex_samplers.push_back(new_tex_sampler);
    }

    return backend_upload_2d_textures(backend, tex_samplers);
}

static std::vector<MeshBuffers> upload_gltf_mesh_buffers(VkBackend* backend, std::span<const GLTFMesh> meshes) {
    std::vector<MeshBuffers> mesh_buffers;
    mesh_buffers.reserve(meshes.size());
    for (const auto& mesh : meshes) {
        MeshBuffers new_mesh_buffers = backend_upload_mesh<Vertex>(backend, mesh.indices, mesh.vertices);
        mesh_buffers.push_back(new_mesh_buffers);
    }

    return mesh_buffers;
}

static uint32_t upload_gltf_materials(VkBackend* backend, std::span<const GLTFMaterial> gltf_materials, uint32_t tex_desc_offset) {

    // If the backend has no materials, create the default material at index 0
    if (backend->mat_count == 0) {
        MaterialData default_mat{};
        default_mat.color_tex_i       = 0;
        default_mat.metal_rough_tex_i = 0;

        std::vector mat_arr = {default_mat};

        std::ignore = backend_upload_materials<MaterialData>(backend, mat_arr);
    }

    std::vector<MaterialData> materials;
    materials.reserve(gltf_materials.size());
    for (const auto& gltf_mat : gltf_materials) {
        MaterialData new_mat_data = gltf_mat.mat_data;
        if (gltf_mat.mat_data.color_tex_i >= 0) {
            new_mat_data.color_tex_i = gltf_mat.mat_data.color_tex_i + tex_desc_offset;
        } else {
            new_mat_data.color_tex_i = 0;
        }
        if (gltf_mat.mat_data.metal_rough_tex_i >= 0) {
            new_mat_data.metal_rough_tex_i = gltf_mat.mat_data.metal_rough_tex_i + tex_desc_offset;
        } else {
            new_mat_data.metal_rough_tex_i = 0;
        }
        materials.push_back(new_mat_data);
    }

    return backend_upload_materials<MaterialData>(backend, materials);

    return 0;
}

Entity load_entity(VkBackend* backend, const std::filesystem::path& path) {

    constexpr auto supported_extensions = fastgltf::Extensions::KHR_mesh_quantization | fastgltf::Extensions::KHR_texture_transform |
                                          fastgltf::Extensions::KHR_materials_clearcoat | fastgltf::Extensions::KHR_materials_specular |
                                          fastgltf::Extensions::KHR_materials_transmission | fastgltf::Extensions::KHR_materials_variants;

    fastgltf::Parser         parser(supported_extensions);
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);

    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                  fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
                                  fastgltf::Options::GenerateMeshIndices;

    auto load = parser.loadGltf(&data, path.parent_path(), gltf_options);

    if (auto error = load.error(); error != fastgltf::Error::None) {
        fmt::println("ERROR LOADING GLTF");
        std::exit(1);
    }
    fastgltf::Asset asset;
    asset = std::move(load.get());

    const std::vector<GLTFImage>    gltf_images     = load_gltf_images(&asset);
    const std::vector<GLTFTexture>  gltf_textures   = load_gltf_textures(&asset);
    const std::vector<GLTFMaterial> gltf_materials  = load_gltf_materials(&asset);
    const std::vector<GLTFMesh>     gltf_meshes     = load_gltf_meshes(&asset);
    const std::vector<GLTFNode>     gltf_mesh_nodes = load_gltf_mesh_nodes(&asset);

    const std::vector<MeshBuffers> vk_meshes       = upload_gltf_mesh_buffers(backend, gltf_meshes);
    const uint32_t                 tex_desc_offset = upload_gltf_textures(backend, gltf_images, gltf_textures, asset.samplers);
    const uint32_t                 mat_desc_offset = upload_gltf_materials(backend, gltf_materials, tex_desc_offset);

    const std::vector<MeshData> mesh_data = create_mesh_data(backend, gltf_mesh_nodes, vk_meshes);

    std::vector<TopLevelInstanceRef> instance_refs;
    instance_refs.reserve(gltf_mesh_nodes.size());
    for (const auto& node : gltf_mesh_nodes) {
        TopLevelInstanceRef new_instance_ref{};
        new_instance_ref.mesh_idx = node.mesh_i;
        // Transposing since vulkan acceleration structure instances expect a row-major matrix
        // and we are in column major
        new_instance_ref.transform = glm::transpose(node.transform);

        instance_refs.push_back(new_instance_ref);
    }
    std::vector<BottomLevelGeometry> bottom_level_geometries;
    bottom_level_geometries.reserve(vk_meshes.size());
    for (size_t i = 0; i < vk_meshes.size(); i++) {
        BottomLevelGeometry new_bottom_level_geometry{};
        new_bottom_level_geometry.mesh_buffers  = vk_meshes[i];
        new_bottom_level_geometry.index_count   = gltf_meshes[i].indices.size();
        new_bottom_level_geometry.vertex_count  = gltf_meshes[i].vertices.size();
        new_bottom_level_geometry.vertex_stride = sizeof(Vertex);
        bottom_level_geometries.push_back(new_bottom_level_geometry);
    }

    backend_create_accel_struct(backend, bottom_level_geometries, instance_refs);

    Entity entity{};
    for (size_t node_i = 0; node_i < gltf_mesh_nodes.size(); node_i++) {
        const GLTFNode* node = &gltf_mesh_nodes[node_i];
        const GLTFMesh* mesh = &gltf_meshes[node->mesh_i];
        for (const auto& primitive : mesh->primitives) {
            DrawObject new_draw_obj{};
            new_draw_obj.mesh_data     = mesh_data[node_i];
            new_draw_obj.index_buffer  = vk_meshes[node->mesh_i].indices.buffer;
            new_draw_obj.indices_count = primitive.indices_count;
            new_draw_obj.indices_start = primitive.indices_start;

            if (primitive.material_i.has_value()) {
                new_draw_obj.mesh_data.mat_i = primitive.material_i.value() + mat_desc_offset;
            } else {
                new_draw_obj.mesh_data.mat_i = 0;
            }

            if (gltf_materials[primitive.material_i.value_or(0)].alpha_mode == fastgltf::AlphaMode::Blend) {
                entity.transparent_objs.push_back(new_draw_obj);
            } else {
                entity.opaque_objs.push_back(new_draw_obj);
            }
        }
    }

    entity.opaque_objs.shrink_to_fit();
    entity.transparent_objs.shrink_to_fit();

    // extract name from path
    entity.name      = path.stem().string();
    entity.path      = path;
    entity.transform = glm::mat4(1.f);

    for (auto& image : gltf_images) {
        stbi_image_free(image.data);
    }

    return entity;
}