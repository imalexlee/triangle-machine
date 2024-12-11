#pragma once

#include "../../thirdparty/shaderc/libshaderc/src/shaderc_private.h"
#include <../../thirdparty/shaderc/libshaderc/include/shaderc/shaderc.h>
#include <iostream>
#include <vulkan/vulkan.h>

std::vector<uint32_t> compile_shader_spv(shaderc_compiler_t compiler, shaderc_compile_options_t compile_options, const std::string& filename,
                                         VkShaderStageFlagBits shader_stage);
