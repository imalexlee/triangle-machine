#pragma once

#include "vk_backend/vk_ext.h"
#include <filesystem>
#include <shaderc/shaderc.h>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Shader {
    VkShaderEXT           shader = VK_NULL_HANDLE;
    std::string           name   = "shader";
    VkShaderStageFlagBits stage;
};

struct ShaderBuilder {
    std::vector<std::string>           names;
    std::vector<VkShaderCreateInfoEXT> create_infos;
    std::vector<std::vector<uint32_t>> spvs;
    shaderc_compiler_t                 compiler;
};

struct ShaderContext {
    std::vector<Shader> vert_shaders;
    std::vector<Shader> frag_shaders;
    ShaderBuilder       builder;
};

enum class ShaderType {
    unlinked,
    linked,
};

void shader_ctx_init(ShaderContext* shader_ctx);

void shader_ctx_init(const ShaderContext* shader_ctx, const ExtContext* ext_ctx, VkDevice device);

void shader_ctx_stage_shader(ShaderContext* shader_ctx, const std::filesystem::path& file_path, const std::string& name,
                             std::span<VkDescriptorSetLayout> desc_set_layouts, std::span<VkPushConstantRange> push_constant_ranges,
                             VkShaderStageFlagBits stage, VkShaderStageFlags next_stage);

void shader_ctx_commit_shaders(ShaderContext* shader_ctx, const ExtContext* ext_ctx, VkDevice device, ShaderType shader_type);
