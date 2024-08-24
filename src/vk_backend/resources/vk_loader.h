#pragma once

#include "vk_backend/vk_scene.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_types.h>

VkShaderModule load_shader_module(VkDevice device, const char* file_path);

Scene load_scene(VkBackend* backend, std::filesystem::path path);

void destroy_scene(VkBackend* backend);

MeshBuffers
upload_mesh_buffers(VkBackend* backend, std::span<uint32_t> indices, std::span<Vertex> vertices);
// downloads texture from external image data (png or jpg) to then load onto the gpu
AllocatedImage
download_texture(const VkBackend* backend, fastgltf::Asset* asset, fastgltf::Texture* gltf_texture);

// uploads texture to gpu memory
AllocatedImage upload_texture(
    const VkBackend* backend, void* data, VkImageUsageFlags usage, uint32_t height, uint32_t width);
