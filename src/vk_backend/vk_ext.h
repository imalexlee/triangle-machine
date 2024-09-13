#pragma once
#include <vulkan/vulkan.h>

struct VkExtContext {

    PFN_vkCreateShadersEXT               vkCreateShadersEXT;
    PFN_vkDestroyShaderEXT               vkDestroyShaderEXT;
    PFN_vkCmdBindShadersEXT              vkCmdBindShadersEXT;
    PFN_vkCmdSetPolygonModeEXT           vkCmdSetPolygonModeEXT;
    PFN_vkCmdSetRasterizationSamplesEXT  vkCmdSetRasterizationSamplesEXT;
    PFN_vkCmdSetSampleMaskEXT            vkCmdSetSampleMaskEXT;
    PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
    PFN_vkCmdSetColorBlendEnableEXT      vkCmdSetColorBlendEnableEXT;
    PFN_vkCmdSetColorBlendEquationEXT    vkCmdSetColorBlendEquationEXT;
    PFN_vkCmdSetColorWriteMaskEXT        vkCmdSetColorWriteMaskEXT;
    PFN_vkCmdSetAlphaToOneEnableEXT      vkCmdSetAlphaToOneEnableEXT;
};

void init_vk_ext_context(VkExtContext* ext_ctx, VkDevice device);
