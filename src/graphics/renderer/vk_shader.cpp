#include "vk_shader.h"

#include "../../thirdparty/shaderc/libshaderc/src/shaderc_private.h"
#include <../../thirdparty/fmt/include/fmt/format.h>
#include <../../thirdparty/shaderc/libshaderc/include/shaderc/shaderc.h>

#include "vk_renderer.h"
#include <assert.h>
#include <fstream>
#include <iostream>
#include <resources/shader_compiler.h>
#include <system/device/vk_utils.h>

static std::string read_file(const std::string& filename);
static void        flush_builder_state(ShaderBuilder* builder);

void shader_ctx_init(ShaderContext* shader_ctx) {

    shader_ctx->builder.shader_options = shaderc_compile_options_initialize();

    shaderc_compile_options_set_target_spirv(shader_ctx->builder.shader_options, shaderc_spirv_version_1_6);
    shaderc_compile_options_set_generate_debug_info(shader_ctx->builder.shader_options);

    shader_ctx->builder.compiler = shaderc_compiler_initialize();
}

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

    std::vector<uint32_t> shader_spv =
        compile_shader_spv(shader_ctx->builder.compiler, shader_ctx->builder.shader_options, file_path.string(), stage);

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

    shader_ctx->builder.create_infos.push_back(vk_shader_ci);
    shader_ctx->builder.spvs.push_back(shader_spv);
    shader_ctx->builder.names.push_back(name);
    shader_ctx->builder.paths.push_back(file_path.string());
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
        VkShaderEXT            shader_ext = shader_exts[i];
        std::string&           name       = shader_ctx->builder.names[i];
        std::string&           path       = shader_ctx->builder.paths[i];
        VkShaderCreateInfoEXT& shader_ci  = shader_ctx->builder.create_infos[i];

        Shader new_shader;
        new_shader.name      = name;
        new_shader.path      = path;
        new_shader.shader    = shader_ext;
        new_shader.shader_ci = shader_ci;
        new_shader.stage     = shader_ctx->builder.create_infos[i].stage;

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

void shader_ctx_replace_shader(ShaderContext* shader_ctx, const ExtContext* ext_ctx, VkDevice device, ShaderType shader_type, uint32_t shader_idx) {
    assert(shader_ctx->builder.create_infos.size() == 1);

    ShaderBuilder* builder = &shader_ctx->builder;
    builder->create_infos[0].flags |= shader_type == ShaderType::linked ? VK_SHADER_CREATE_LINK_STAGE_BIT_EXT : 0;
    builder->create_infos[0].pCode    = builder->spvs[0].data();
    builder->create_infos[0].codeSize = builder->spvs[0].size() * sizeof(uint32_t);

    Shader* shader = nullptr;
    switch (builder->create_infos[0].stage) {
    case VK_SHADER_STAGE_VERTEX_BIT:
        shader = &shader_ctx->vert_shaders[shader_idx];
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        shader = &shader_ctx->frag_shaders[shader_idx];
        break;
    default:
        std::cerr << "shader stage " << shader_ctx->builder.create_infos[0].stage << "not handled\n";
        return;
    }

    ext_ctx->vkDestroyShaderEXT(device, shader->shader, nullptr);

    VK_CHECK(ext_ctx->vkCreateShadersEXT(device, 1, shader_ctx->builder.create_infos.data(), nullptr, &shader->shader));

    flush_builder_state(&shader_ctx->builder);
}

void flush_builder_state(ShaderBuilder* builder) {
    builder->names.clear();
    builder->paths.clear();
    builder->create_infos.clear();
    builder->spvs.clear();
}
