#pragma once
#include <filesystem>
#include <scene/scene.h>

[[nodiscard]] Entity load_entity(struct Renderer* backend, const std::filesystem::path& path);
