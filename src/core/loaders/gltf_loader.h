#pragma once

#include "stb_image.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_types.h>

#include "fastgltf/types.hpp"
#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_image.h"
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

[[nodiscard]] Entity load_scene(VkBackend* backend, std::filesystem::path path);

// void destroy_scene(VkBackend* backend);

//  downloads texture from external image data (png or jpg) to then load onto the gpu
[[nodiscard]] AllocatedImage
download_texture(const VkBackend* backend, fastgltf::Asset* asset, fastgltf::Texture* gltf_texture);
