#include "vk_ext.h"

void init_vk_ext_context(VkExtContext* ext_ctx, VkDevice device) {
    ext_ctx->vkCreateShadersEXT =
        reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(device, "vkCreateShadersEXT"));
    ext_ctx->vkDestroyShaderEXT =
        reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(device, "vkDestroyShaderEXT"));
    ext_ctx->vkCmdBindShadersEXT = reinterpret_cast<PFN_vkCmdBindShadersEXT>(
        vkGetDeviceProcAddr(device, "vkCmdBindShadersEXT"));
    ext_ctx->vkCmdSetPolygonModeEXT = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetPolygonModeEXT"));
    ext_ctx->vkCmdSetRasterizationSamplesEXT =
        reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(
            vkGetDeviceProcAddr(device, "vkCmdSetRasterizationSamplesEXT"));
    ext_ctx->vkCmdSetSampleMaskEXT = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetSampleMaskEXT"));
    ext_ctx->vkCmdSetAlphaToCoverageEnableEXT =
        reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(
            vkGetDeviceProcAddr(device, "vkCmdSetAlphaToCoverageEnableEXT"));
    ext_ctx->vkCmdSetColorBlendEnableEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT"));
    ext_ctx->vkCmdSetColorWriteMaskEXT = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetColorWriteMaskEXT"));
    ext_ctx->vkCmdSetColorBlendEquationEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT"));
    ext_ctx->vkCmdSetAlphaToOneEnableEXT = reinterpret_cast<PFN_vkCmdSetAlphaToOneEnableEXT>(
        vkGetDeviceProcAddr(device, "vkCmdSetAlphaToOneEnableEXT"));
}
