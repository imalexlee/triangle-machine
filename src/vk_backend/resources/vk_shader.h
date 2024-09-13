#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VkBackend;

struct Shader {
    VkShaderEXT           shader = VK_NULL_HANDLE;
    std::string           name   = "shader";
    VkShaderStageFlagBits stage;
};

struct ShaderBuilder {
    std::vector<std::string>           names;
    std::vector<VkShaderCreateInfoEXT> create_infos;
    std::vector<std::vector<uint32_t>> spvs;
};

void add_shader(ShaderBuilder* builder, const std::filesystem::path& file_path,
                const std::string& name, std::span<VkDescriptorSetLayout> desc_set_layouts,
                std::span<VkPushConstantRange> push_constant_ranges, VkShaderStageFlagBits stage,
                VkShaderStageFlags next_stage);

std::vector<Shader> create_linked_shaders(ShaderBuilder* builder, const VkBackend* backend);

std::vector<Shader> create_unlinked_shaders(ShaderBuilder* builder, const VkBackend* backend);
