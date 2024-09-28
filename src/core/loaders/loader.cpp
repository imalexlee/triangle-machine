#include "loader.h"

#include "stb_image.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <iostream>
#include <vk_backend/vk_backend.h>

constexpr uint32_t tex_coord_num = 2;
struct Vertex {
    glm::vec4 position{};
    glm::vec4 normal{};
    glm::vec2 tex_coord[tex_coord_num] = {
        {0, 0},
        {0, 0}
    };
};

struct Mesh {
    std::vector<uint32_t> indices{};
    std::vector<Vertex>   vertices{};
    uint32_t              mat_idx;
};

struct Material {
    uint32_t tex_i;
};

struct Texture {
    const char* path{};
    uint8_t*    data{};
    uint32_t    width{};
    uint32_t    height{};
    uint32_t    color_channels{};
};

std::vector<Texture> load_textures_2(const aiScene* scene) {
    std::vector<Texture> textures;
    textures.reserve(scene->mNumTextures);
    for (uint32_t tex_i = 0; tex_i < scene->mNumTextures; tex_i++) {
        aiTexture*  ai_tex = scene->mTextures[tex_i];
        const char* path   = ai_tex->mFilename.C_Str();
        int         width, height, num_channels;
        uint8_t*    img_data = stbi_load(path, &width, &height, &num_channels, 4);
        assert(img_data);
        assert(width == ai_tex->mWidth && height == ai_tex->mHeight);

        Texture new_texture;
        new_texture.path           = path;
        new_texture.data           = img_data;
        new_texture.width          = width;
        new_texture.height         = height;
        new_texture.color_channels = 4;

        textures.push_back(new_texture);
    }
}

uint32_t tex_index_from_path(const aiScene* scene, aiString path) {
    for (uint32_t i = 0; i < scene->mNumTextures; i++) {
        const aiTexture* texture = scene->mTextures[i];
        if (strcmp(texture->mFilename.C_Str(), path.C_Str()) == 0) {
            return i;
        }
    }
    __builtin_unreachable();
}

std::vector<Material> load_materials_2(const aiScene* scene, aiTextureType type) {
    for (uint32_t mat_i = 0; mat_i < scene->mNumMaterials; mat_i++) {
        const aiMaterial* ai_mat = scene->mMaterials[mat_i];

        aiColor4D base_color(0, 0, 0, 0);
        aiGetMaterialColor(ai_mat, AI_MATKEY_COLOR_DIFFUSE, &base_color);
        for (uint32_t tex_type_i = 0; tex_type_i < ai_mat->GetTextureCount(type); tex_type_i++) {
            aiString         path;
            aiTextureMapping mapping;
            uint32_t         tex_coord;
            ai_mat->GetTexture(type, tex_type_i, &path, &mapping, &tex_coord);

            Material new_material;
            new_material.tex_i = tex_index_from_path(scene, path);
        }
    }
}
std::vector<Mesh> load_meshes_2(const aiScene* scene) {
    std::vector<Mesh> meshes;
    meshes.reserve(scene->mNumMeshes);
    for (uint32_t ai_mesh_i = 0; ai_mesh_i < scene->mNumMeshes; ai_mesh_i++) {
        const aiMesh* ai_mesh = scene->mMeshes[ai_mesh_i];

        Mesh new_mesh{};
        new_mesh.vertices.reserve(ai_mesh->mNumVertices);
        for (uint32_t vert_i = 0; vert_i < ai_mesh->mNumVertices; vert_i++) {
            Vertex new_vertex;

            new_vertex.position     = {ai_mesh->mVertices[vert_i].x, ai_mesh->mVertices[vert_i].y, ai_mesh->mVertices[vert_i].z, 1.f};
            new_vertex.normal       = {ai_mesh->mNormals[vert_i].x, ai_mesh->mNormals[vert_i].y, ai_mesh->mNormals[vert_i].z, 1.f};
            new_vertex.tex_coord[0] = {ai_mesh->mTextureCoords[vert_i][0].x, ai_mesh->mTextureCoords[vert_i][0].y};
            new_vertex.tex_coord[1] = {ai_mesh->mTextureCoords[vert_i][1].x, ai_mesh->mTextureCoords[vert_i][1].y};
            new_mesh.vertices.push_back(new_vertex);
        }

        for (uint32_t face_i = 0; face_i < ai_mesh->mNumFaces; face_i++) {
            const aiFace* ai_face = &ai_mesh->mFaces[face_i];
            assert(ai_face->mNumIndices == 3);
            new_mesh.indices.push_back(ai_face->mIndices[0]);
            new_mesh.indices.push_back(ai_face->mIndices[1]);
            new_mesh.indices.push_back(ai_face->mIndices[2]);
        }

        new_mesh.mat_idx = ai_mesh->mMaterialIndex;

        meshes.push_back(new_mesh);
    }

    return meshes;
};

Entity load_entity_2(VkBackend* backend, const std::filesystem::path& path) {

    int ai_flags = aiProcess_Triangulate | aiProcess_GenUVCoords | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices;

    const aiScene* scene = aiImportFile(path.c_str(), ai_flags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "Assimp Error: " << aiGetErrorString() << std::endl;
        // TODO: handle error
        exit(1);
    }

    std::vector<Texture>  textures        = load_textures_2(scene);
    std::vector<Material> base_color_mats = load_materials_2(scene, aiTextureType_BASE_COLOR);
    std::vector<Mesh>     meshes          = load_meshes_2(scene);

    aiReleaseImport(scene);
}