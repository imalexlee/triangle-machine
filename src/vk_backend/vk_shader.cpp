#include "vk_shader.h"

#include "../../thirdparty/shaderc/libshaderc/src/shaderc_private.h"
#include <../../thirdparty/fmt/include/fmt/format.h>
#include <../../thirdparty/shaderc/libshaderc/include/shaderc/shaderc.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <vk_backend/vk_backend.h>
#include <vk_backend/vk_utils.h>

static std::string           read_file(const std::string& filename);
static void                  flush_builder_state(ShaderBuilder* builder);
static std::vector<uint32_t> compile_shader_spv(shaderc_compiler_t compiler, const std::string& filename, VkShaderStageFlagBits shader_stage);

void shader_ctx_init(ShaderContext* shader_ctx) { shader_ctx->builder.compiler = shaderc_compiler_initialize(); }

void shader_ctx_deinit(const ShaderContext* shader_ctx, const ExtContext* ext_ctx, VkDevice device) {
    shaderc_compiler_release(shader_ctx->builder.compiler);

    for (const auto& vert_shader : shader_ctx->vert_shaders) {
        ext_ctx->vkDestroyShaderEXT(device, vert_shader.shader, nullptr);
    }

    for (const auto& frag_shader : shader_ctx->frag_shaders) {
        ext_ctx->vkDestroyShaderEXT(device, frag_shader.shader, nullptr);
    }
}

void shader_ctx_stage_shader(ShaderContext* shader_ctx, const std::filesystem::path& file_path, const std::string& name,
                             std::span<VkDescriptorSetLayout> desc_set_layouts, std::span<VkPushConstantRange> push_constant_ranges,
                             VkShaderStageFlagBits stage, VkShaderStageFlags next_stage) {

    std::vector<uint32_t> shader_spv = compile_shader_spv(shader_ctx->builder.compiler, file_path, stage);

    VkShaderCreateInfoEXT vk_shader_ci{};
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

    shader_ctx->builder.names.push_back(name);
    shader_ctx->builder.create_infos.push_back(vk_shader_ci);
    shader_ctx->builder.spvs.push_back(shader_spv);
}

void shader_ctx_commit_shaders(ShaderContext* shader_ctx, const ExtContext* ext_ctx, VkDevice device, ShaderType shader_type) {
    std::vector<VkShaderEXT> shader_exts{};
    shader_exts.resize(shader_ctx->builder.create_infos.size());

    for (size_t i = 0; i < shader_ctx->builder.create_infos.size(); i++) {
        shader_ctx->builder.create_infos[i].flags |= shader_type == ShaderType::linked ? VK_SHADER_CREATE_LINK_STAGE_BIT_EXT : 0;
        shader_ctx->builder.create_infos[i].pCode    = shader_ctx->builder.spvs[i].data();
        shader_ctx->builder.create_infos[i].codeSize = shader_ctx->builder.spvs[i].size() * sizeof(uint32_t);
    }

    VK_CHECK(ext_ctx->vkCreateShadersEXT(device, shader_ctx->builder.create_infos.size(), shader_ctx->builder.create_infos.data(), nullptr,
                                         shader_exts.data()));

    for (size_t i = 0; i < shader_exts.size(); i++) {
        VkShaderEXT shader_ext = shader_exts[i];
        std::string name       = shader_ctx->builder.names[i];

        Shader new_shader;
        new_shader.name   = name;
        new_shader.shader = shader_ext;
        new_shader.stage  = shader_ctx->builder.create_infos[i].stage;

        switch (shader_ctx->builder.create_infos[i].stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            shader_ctx->vert_shaders.push_back(new_shader);
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            shader_ctx->frag_shaders.push_back(new_shader);
            break;
        default:
            std::cerr << "shader stage " << shader_ctx->builder.create_infos[i].stage << "not handled\n";
        }
    }

    flush_builder_state(&shader_ctx->builder);
}

void flush_builder_state(ShaderBuilder* builder) {
    builder->names.clear();
    builder->create_infos.clear();
    builder->spvs.clear();
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
                std::string filename        = content.substr(quote_start + 1, quote_end - quote_start - 1);
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

std::vector<uint32_t> compile_shader_spv(shaderc_compiler_t compiler, const std::string& filename, VkShaderStageFlagBits shader_stage) {

    std::string shader_source = parse_shader_file(filename);

    auto shader_kind = static_cast<shaderc_shader_kind>(shader_stage >> 1);

    shaderc_compilation_result_t result =
        shaderc_compile_into_spv(compiler, shader_source.data(), shader_source.size(), shader_kind, filename.data(), "main", nullptr);

    std::vector<uint32_t> spv;
    spv.resize(result->output_data_size / sizeof(uint32_t));
    memcpy(spv.data(), result->GetBytes(), result->output_data_size);

    shaderc_result_release(result);

    return spv;
}