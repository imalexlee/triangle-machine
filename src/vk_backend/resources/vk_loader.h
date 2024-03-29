#pragma once

#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

VkShaderModule load_shader_module(VkDevice device, const char* file_path);
