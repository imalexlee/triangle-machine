#pragma once
#include <filesystem>
#include <glslang/Include/glslang_c_shader_types.h>
#include <vector>
int compile_shader(const std::filesystem::path& file_path, glslang_stage_t stage,
                   std::vector<uint32_t>* output);
