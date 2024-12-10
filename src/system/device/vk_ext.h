#pragma once
#include <vulkan/vulkan.h>

struct ExtContext {
    PFN_vkCreateShadersEXT                         vkCreateShadersEXT{};
    PFN_vkDestroyShaderEXT                         vkDestroyShaderEXT{};
    PFN_vkCmdBindShadersEXT                        vkCmdBindShadersEXT{};
    PFN_vkCmdSetPolygonModeEXT                     vkCmdSetPolygonModeEXT{};
    PFN_vkCmdSetRasterizationSamplesEXT            vkCmdSetRasterizationSamplesEXT{};
    PFN_vkCmdSetSampleMaskEXT                      vkCmdSetSampleMaskEXT{};
    PFN_vkCmdSetAlphaToCoverageEnableEXT           vkCmdSetAlphaToCoverageEnableEXT{};
    PFN_vkCmdSetColorBlendEnableEXT                vkCmdSetColorBlendEnableEXT{};
    PFN_vkCmdSetColorBlendEquationEXT              vkCmdSetColorBlendEquationEXT{};
    PFN_vkCmdSetColorWriteMaskEXT                  vkCmdSetColorWriteMaskEXT{};
    PFN_vkCmdSetAlphaToOneEnableEXT                vkCmdSetAlphaToOneEnableEXT{};
    PFN_vkCmdSetVertexInputEXT                     vkCmdSetVertexInputEXT{};
    PFN_vkCmdSetLogicOpEnableEXT                   vkCmdSetLogicOpEnableEXT{};
    PFN_vkCmdSetTessellationDomainOriginEXT        vkCmdSetTessellationDomainOriginEXT{};
    PFN_vkCmdSetPatchControlPointsEXT              vkCmdSetPatchControlPointsEXT{};
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR{};
    PFN_vkBuildAccelerationStructuresKHR           vkBuildAccelerationStructuresKHR{};
    PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR{};
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR{};
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR{};
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR{};
};

void ext_context_init(ExtContext* ext_ctx, VkDevice device);
