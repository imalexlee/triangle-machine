#pragma once

#include "stb_image.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_types.h>

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/resources/vk_image.h"
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

[[nodiscard]] Entity load_entity(VkBackend* backend, std::filesystem::path path);
