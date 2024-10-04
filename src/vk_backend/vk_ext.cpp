#include "vk_ext.h"

void ext_context_init(ExtContext* ext_ctx, VkDevice device) {
    ext_ctx->vkCreateShadersEXT     = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(device, "vkCreateShadersEXT"));
    ext_ctx->vkDestroyShaderEXT     = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(device, "vkDestroyShaderEXT"));
    ext_ctx->vkCmdBindShadersEXT    = reinterpret_cast<PFN_vkCmdBindShadersEXT>(vkGetDeviceProcAddr(device, "vkCmdBindShadersEXT"));
    ext_ctx->vkCmdSetPolygonModeEXT = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(vkGetDeviceProcAddr(device, "vkCmdSetPolygonModeEXT"));
    ext_ctx->vkCmdSetRasterizationSamplesEXT =
        reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(vkGetDeviceProcAddr(device, "vkCmdSetRasterizationSamplesEXT"));
    ext_ctx->vkCmdSetSampleMaskEXT = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(vkGetDeviceProcAddr(device, "vkCmdSetSampleMaskEXT"));
    ext_ctx->vkCmdSetAlphaToCoverageEnableEXT =
        reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetAlphaToCoverageEnableEXT"));
    ext_ctx->vkCmdSetColorBlendEnableEXT =
        reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT"));
    ext_ctx->vkCmdSetColorWriteMaskEXT = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorWriteMaskEXT"));
    ext_ctx->vkCmdSetColorBlendEquationEXT =
        reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT"));
    ext_ctx->vkCmdSetAlphaToOneEnableEXT =
        reinterpret_cast<PFN_vkCmdSetAlphaToOneEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetAlphaToOneEnableEXT"));
    ext_ctx->vkCmdSetVertexInputEXT   = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(device, "vkCmdSetVertexInputEXT"));
    ext_ctx->vkCmdSetLogicOpEnableEXT = reinterpret_cast<PFN_vkCmdSetLogicOpEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetLogicOpEnableEXT"));
    ext_ctx->vkCmdSetTessellationDomainOriginEXT =
        reinterpret_cast<PFN_vkCmdSetTessellationDomainOriginEXT>(vkGetDeviceProcAddr(device, "vkCmdSetTessellationDomainOriginEXT"));
    ext_ctx->vkCmdSetPatchControlPointsEXT =
        reinterpret_cast<PFN_vkCmdSetPatchControlPointsEXT>(vkGetDeviceProcAddr(device, "vkCmdSetPatchControlPointsEXT"));
    ext_ctx->vkCreateAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    ext_ctx->vkBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
    ext_ctx->vkGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    ext_ctx->vkCmdBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    ext_ctx->vkGetAccelerationStructureDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    ext_ctx->vkDestroyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
}
