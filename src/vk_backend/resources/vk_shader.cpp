#include "vk_shader.h"

#include "../../../thirdparty/shaderc/libshaderc/src/shaderc_private.h"

#include <assert.h>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <shaderc/shaderc.h>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_utils.h>

static std::string    read_file(const std::string& filename);
std::vector<uint32_t> compile_shader_spv(const std::string&    filename,
                                         VkShaderStageFlagBits shader_stage);

std::vector<Shader> create_linked_shaders(ShaderBuilder* builder, const VkBackend* backend) {

    for (auto& creation_info : builder->create_infos) {
        creation_info.flags |= VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    }

    std::vector<VkShaderEXT> shader_exts;
    shader_exts.resize(builder->create_infos.size());

    VK_CHECK(backend->ext_ctx.vkCreateShadersEXT(
        backend->device_ctx.logical_device, builder->create_infos.size(),
        builder->create_infos.data(), nullptr, shader_exts.data()));

    std::vector<Shader> shaders;
    shaders.reserve(shader_exts.size());
    for (size_t i = 0; i < shader_exts.size(); i++) {
        VkShaderEXT shader_ext = shader_exts[i];
        std::string name       = builder->names[i];

        Shader new_shader;
        new_shader.name   = name;
        new_shader.shader = shader_ext;
        new_shader.stage  = builder->create_infos[i].stage;
        shaders.push_back(new_shader);
    }

    return shaders;
}

std::vector<Shader> create_unlinked_shaders(ShaderBuilder* builder, const VkBackend* backend) {
    std::vector<VkShaderEXT> shader_exts;
    shader_exts.resize(builder->create_infos.size());

    for (size_t i = 0; i < builder->create_infos.size(); i++) {
        builder->create_infos[i].pCode    = builder->spvs[i].data();
        builder->create_infos[i].codeSize = builder->spvs[i].size() * sizeof(uint32_t);
    }

    VK_CHECK(backend->ext_ctx.vkCreateShadersEXT(backend->device_ctx.logical_device, 1,
                                                 &builder->create_infos[0], nullptr,
                                                 &shader_exts[0]));

    std::vector<Shader> shaders;
    shaders.reserve(shader_exts.size());
    for (size_t i = 0; i < shader_exts.size(); i++) {
        VkShaderEXT shader_ext = shader_exts[i];
        std::string name       = builder->names[i];

        Shader new_shader;
        new_shader.name   = name;
        new_shader.shader = shader_ext;
        new_shader.stage  = builder->create_infos[i].stage;
        shaders.push_back(new_shader);
    }

    return shaders;
}

void add_shader(ShaderBuilder* builder, const std::filesystem::path& file_path,
                const std::string& name, std::span<VkDescriptorSetLayout> desc_set_layouts,
                std::span<VkPushConstantRange> push_constant_ranges, VkShaderStageFlagBits stage,
                VkShaderStageFlags next_stage) {
    Shader new_shader{};
    new_shader.name = name;

    std::vector<uint32_t> shader_spv = compile_shader_spv(file_path, stage);

    VkShaderCreateInfoEXT vk_shader_ci;
    vk_shader_ci.sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    vk_shader_ci.pNext                  = nullptr;
    vk_shader_ci.flags                  = 0;
    vk_shader_ci.stage                  = stage;
    vk_shader_ci.nextStage              = next_stage;
    vk_shader_ci.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    vk_shader_ci.pName                  = "main";
    vk_shader_ci.setLayoutCount         = desc_set_layouts.size();
    vk_shader_ci.pSetLayouts            = desc_set_layouts.data();
    vk_shader_ci.pushConstantRangeCount = push_constant_ranges.size();
    vk_shader_ci.pPushConstantRanges    = push_constant_ranges.data();
    vk_shader_ci.pSpecializationInfo    = nullptr;

    builder->names.push_back(name);
    builder->create_infos.push_back(vk_shader_ci);
    builder->spvs.push_back(shader_spv);
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    assert(file.is_open());
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}

std::string process_includes(std::string content, const std::string& base_dir) {
    bool found_include = true;
    while (found_include) {
        found_include = false;
        size_t pos    = 0;
        while ((pos = content.find("#include", pos)) != std::string::npos) {
            size_t start = pos;
            size_t end   = content.find('\n', start);
            if (end == std::string::npos) {
                end = content.length();
            }
            size_t quote_start = content.find('"', start);
            size_t quote_end   = content.find('"', quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                std::string filename = content.substr(quote_start + 1, quote_end - quote_start - 1);
                std::string full_path       = base_dir + "/" + filename;
                std::string include_content = read_file(full_path);
                content.replace(start, end - start, include_content);
                found_include = true;
            }
            pos = end;
        }
    }
    return content;
}

std::string parse_shader_file(const std::string& filename) {
    std::string content  = read_file(filename);
    std::string base_dir = filename.substr(0, filename.find_last_of("/"));
    // simple text replacement of #includes when compiling shaders at runtime
    content = process_includes(content, base_dir);
    return content;
}

std::vector<uint32_t> compile_shader_spv(const std::string&    filename,
                                         VkShaderStageFlagBits shader_stage) {

    std::string shader_source = parse_shader_file(filename);

    shaderc_compiler_t compiler = shaderc_compiler_initialize();

    auto shader_kind = static_cast<shaderc_shader_kind>(shader_stage >> 1);

    shaderc_compilation_result_t result =
        shaderc_compile_into_spv(compiler, shader_source.data(), shader_source.size(), shader_kind,
                                 filename.data(), "main", nullptr);

    std::vector<uint32_t> spv;
    spv.resize(result->output_data_size / sizeof(uint32_t));
    memcpy(spv.data(), result->GetBytes(), result->output_data_size);

    shaderc_result_release(result);
    shaderc_compiler_release(compiler);

    return spv;
}