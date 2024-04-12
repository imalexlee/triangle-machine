#pragma once

#include "vk_backend/vk_scene.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_types.h>

VkShaderModule load_shader_module(VkDevice device, const char* file_path);

GLTFScene load_scene(VkBackend* backend, std::filesystem::path path);
