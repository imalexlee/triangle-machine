#include "shader_compiler.h"
#include <system/host/io.h>

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

std::vector<uint32_t> compile_shader_spv(shaderc_compiler_t compiler, shaderc_compile_options_t compile_options, const std::string& filename,
                                         VkShaderStageFlagBits shader_stage) {

    std::string shader_source = parse_shader_file(filename);

    auto shader_kind = static_cast<shaderc_shader_kind>(shader_stage >> 1);

    shaderc_compilation_result_t result =
        shaderc_compile_into_spv(compiler, shader_source.data(), shader_source.size(), shader_kind, filename.data(), "main", compile_options);
    if (result->num_errors > 0) {
        std::cout << result->messages << std::endl;
        exit(EXIT_FAILURE);
    }

    std::vector<uint32_t> spv;
    spv.resize(result->output_data_size / sizeof(uint32_t));
    memcpy(spv.data(), result->GetBytes(), result->output_data_size);

    shaderc_result_release(result);

    return spv;
}
