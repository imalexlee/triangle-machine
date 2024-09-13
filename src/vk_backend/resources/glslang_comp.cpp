
/*
#include "../../../thirdparty/glslang/glslang/Public/ShaderLang.h"

#include <../../../thirdparty/glslang/glslang/Include/glslang_c_interface.h>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
static std::string parse_shader_file(const std::string& filename);

glslang_resource_t glslang_resources = {
    .max_lights                                    = 32,
    .max_clip_planes                               = 6,
    .max_texture_units                             = 32,
    .max_texture_coords                            = 32,
    .max_vertex_attribs                            = 64,
    .max_vertex_uniform_components                 = 4096,
    .max_varying_floats                            = 64,
    .max_vertex_texture_image_units                = 32,
    .max_combined_texture_image_units              = 80,
    .max_texture_image_units                       = 32,
    .max_fragment_uniform_components               = 4096,
    .max_draw_buffers                              = 32,
    .max_vertex_uniform_vectors                    = 128,
    .max_varying_vectors                           = 8,
    .max_fragment_uniform_vectors                  = 16,
    .max_vertex_output_vectors                     = 16,
    .max_fragment_input_vectors                    = 15,
    .min_program_texel_offset                      = 8,
    .max_program_texel_offset                      = 7,
    .max_clip_distances                            = 8,
    .max_compute_work_group_count_x                = 65535,
    .max_compute_work_group_count_y                = 65535,
    .max_compute_work_group_count_z                = 65535,
    .max_compute_work_group_size_x                 = 1024,
    .max_compute_work_group_size_y                 = 1024,
    .max_compute_work_group_size_z                 = 64,
    .max_compute_uniform_components                = 1024,
    .max_compute_texture_image_units               = 16,
    .max_compute_image_uniforms                    = 8,
    .max_compute_atomic_counters                   = 8,
    .max_compute_atomic_counter_buffers            = 1,
    .max_varying_components                        = 60,
    .max_vertex_output_components                  = 64,
    .max_geometry_input_components                 = 64,
    .max_geometry_output_components                = 128,
    .max_fragment_input_components                 = 128,
    .max_image_units                               = 8,
    .max_combined_image_units_and_fragment_outputs = 8,
    .max_combined_shader_output_resources          = 8,
    .max_image_samples                             = 0,
    .max_vertex_image_uniforms                     = 0,
    .max_tess_control_image_uniforms               = 0,
    .max_tess_evaluation_image_uniforms            = 0,
    .max_geometry_image_uniforms                   = 0,
    .max_fragment_image_uniforms                   = 8,
    .max_combined_image_uniforms                   = 8,
    .max_geometry_texture_image_units              = 16,
    .max_geometry_output_vertices                  = 256,
    .max_geometry_total_output_components          = 1024,
    .max_geometry_uniform_components               = 1024,
    .max_geometry_varying_components               = 64,
    .max_tess_control_input_components             = 128,
    .max_tess_control_output_components            = 128,
    .max_tess_control_texture_image_units          = 16,
    .max_tess_control_uniform_components           = 1024,
    .max_tess_control_total_output_components      = 4096,
    .max_tess_evaluation_input_components          = 128,
    .max_tess_evaluation_output_components         = 128,
    .max_tess_evaluation_texture_image_units       = 16,
    .max_tess_evaluation_uniform_components        = 1024,
    .max_tess_patch_components                     = 120,
    .max_patch_vertices                            = 32,
    .max_tess_gen_level                            = 64,
    .max_viewports                                 = 16,
    .max_vertex_atomic_counters                    = 0,
    .max_tess_control_atomic_counters              = 0,
    .max_tess_evaluation_atomic_counters           = 0,
    .max_geometry_atomic_counters                  = 0,
    .max_fragment_atomic_counters                  = 8,
    .max_combined_atomic_counters                  = 8,
    .max_atomic_counter_bindings                   = 1,
    .max_vertex_atomic_counter_buffers             = 0,
    .max_tess_control_atomic_counter_buffers       = 0,
    .max_tess_evaluation_atomic_counter_buffers    = 0,
    .max_geometry_atomic_counter_buffers           = 0,
    .max_fragment_atomic_counter_buffers           = 1,
    .max_combined_atomic_counter_buffers           = 1,
    .max_atomic_counter_buffer_size                = 16384,
    .max_transform_feedback_buffers                = 4,
    .max_transform_feedback_interleaved_components = 64,
    .max_cull_distances                            = 8,
    .max_combined_clip_and_cull_distances          = 8,
    .max_samples                                   = 4,
    .max_mesh_output_vertices_nv                   = 256,
    .max_mesh_output_primitives_nv                 = 512,
    .max_mesh_work_group_size_x_nv                 = 32,
    .max_mesh_work_group_size_y_nv                 = 1,
    .max_mesh_work_group_size_z_nv                 = 1,
    .max_task_work_group_size_x_nv                 = 32,
    .max_task_work_group_size_y_nv                 = 1,
    .max_task_work_group_size_z_nv                 = 1,
    .max_mesh_view_count_nv                        = 4,

    .limits = {
               .non_inductive_for_loops                  = true,
               .while_loops                              = true,
               .do_while_loops                           = true,
               .general_uniform_indexing                 = true,
               .general_attribute_matrix_vector_indexing = true,
               .general_varying_indexing                 = true,
               .general_sampler_indexing                 = true,
               .general_variable_indexing                = true,
               .general_constant_matrix_vector_indexing  = true,
               }
};

int compile_shader(const std::filesystem::path file_path, VkShaderStageFlagBits stage,
                   std::vector<uint32_t>* output) {

    std::string shader_source = parse_shader_file(file_path);

    // see type of glslang_stage_t to see why I right shift
    auto glslang_stage = static_cast<glslang_stage_t>(stage >> 1);

    const glslang_input_t glslang_input = {
        .language                          = GLSLANG_SOURCE_GLSL,
        .stage                             = glslang_stage,
        .client                            = GLSLANG_CLIENT_VULKAN,
        .client_version                    = GLSLANG_TARGET_VULKAN_1_3,
        .target_language                   = GLSLANG_TARGET_SPV,
        .target_language_version           = GLSLANG_TARGET_SPV_1_6,
        .code                              = shader_source.data(),
        .default_version                   = 460,
        .default_profile                   = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible                = false,
        .messages                          = GLSLANG_MSG_DEFAULT_BIT,
        .resource                          = &glslang_resources,

    };

    glslang_shader_t* shader = glslang_shader_create(&glslang_input);
    assert(shader);

    if (!glslang_shader_preprocess(shader, &glslang_input)) {
        std::cerr << "GLSL Preprocessing Failed\n";
        std::cerr << glslang_shader_get_info_log(shader) << "\n";
        std::cerr << glslang_shader_get_info_debug_log(shader) << "\n";
        std::cerr << file_path << '\n';
        return -1;
    }

    if (!glslang_shader_parse(shader, &glslang_input)) {
        std::cerr << "GLSL Parsing Failed\n";
        std::cerr << glslang_shader_get_info_log(shader) << "\n";
        std::cerr << glslang_shader_get_info_debug_log(shader) << "\n";
        std::cerr << file_path << '\n';
        return -1;
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    int msgs = GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT;

    if (!glslang_program_link(program, msgs)) {
        std::cerr << "GLSL Linking Failed\n";
        std::cerr << glslang_program_get_info_log(program) << "\n";
        std::cerr << glslang_program_get_info_debug_log(program) << "\n";
        std::cerr << file_path << '\n';
        return -1;
    }

    glslang_program_SPIRV_generate(program, glslang_stage);
    output->resize(glslang_program_SPIRV_get_size(program));
    glslang_program_SPIRV_get(program, output->data());
    return 1;
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
*/
