#pragma once
#include <filesystem>
#include <vk_backend/vk_backend.h>

[[nodiscard]] Entity load_entity(const VkBackend* backend, const std::filesystem::path& path);
