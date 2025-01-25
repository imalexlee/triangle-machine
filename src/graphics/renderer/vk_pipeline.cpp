#include "vk_pipeline.h"
#include "system/device/vk_utils.h"

#include <array>
#include <fmt/base.h>
#include <vulkan/vulkan_core.h>

PipelineInfo pipeline_builder_create_pipeline(const PipelineBuilder* pb, VkDevice device) {
    VkPipeline new_pipeline;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
    vertex_input_ci.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_ci.vertexBindingDescriptionCount   = 0;
    vertex_input_ci.vertexAttributeDescriptionCount = 0;

    std::array dynamic_states{VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};

    VkPipelineDynamicStateCreateInfo dynamic_ci{};
    dynamic_ci.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_ci.pDynamicStates    = dynamic_states.data();
    dynamic_ci.dynamicStateCount = dynamic_states.size();

    // not setting the viewport and scissor as its dynamic state
    VkPipelineViewportStateCreateInfo viewport_ci{};
    viewport_ci.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_ci.scissorCount  = 1;
    viewport_ci.viewportCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    color_blend_state.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.pNext           = nullptr;
    color_blend_state.logicOpEnable   = VK_FALSE;
    color_blend_state.logicOp         = VK_LOGIC_OP_COPY;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments    = &pb->color_blend_attachment;

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(device, &pb->pipeline_layout_ci, nullptr, &pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.pNext               = &pb->rendering_ci; // for dynamic rendering
    pipeline_ci.pStages             = pb->shader_stages.data();
    pipeline_ci.stageCount          = pb->shader_stages.size();
    pipeline_ci.pVertexInputState   = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &pb->input_assembly_state;
    pipeline_ci.pDynamicState       = &dynamic_ci;
    pipeline_ci.pViewportState      = &viewport_ci;
    pipeline_ci.pRasterizationState = &pb->rasterization_state;
    pipeline_ci.pMultisampleState   = &pb->multisample_state;
    pipeline_ci.pDepthStencilState  = &pb->depth_stencil_state;
    pipeline_ci.pColorBlendState    = &color_blend_state;
    pipeline_ci.layout              = pipeline_layout;

    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_ci, nullptr, &new_pipeline));
    return {
        .pipeline        = new_pipeline,
        .pipeline_layout = pipeline_layout,
    };
}

VkPipelineLayout vk_pipeline_layout_create(VkDevice device, std::span<VkDescriptorSetLayout> desc_set_layouts,
                                           std::span<VkPushConstantRange> push_constant_ranges, VkPipelineLayoutCreateFlags flags) {

    VkPipelineLayoutCreateInfo pipeline_layout_ci{};
    pipeline_layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_ci.flags                  = flags;
    pipeline_layout_ci.pSetLayouts            = desc_set_layouts.data();
    pipeline_layout_ci.setLayoutCount         = desc_set_layouts.size();
    pipeline_layout_ci.pPushConstantRanges    = push_constant_ranges.data();
    pipeline_layout_ci.pushConstantRangeCount = push_constant_ranges.size();

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout));

    return pipeline_layout;
};

void pipeline_builder_set_render_state(PipelineBuilder* pb, VkFormat color_format, VkFormat depth_format) {
    pb->color_attachment_format              = color_format;
    pb->rendering_ci                         = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pb->rendering_ci.pColorAttachmentFormats = &pb->color_attachment_format;
    pb->rendering_ci.colorAttachmentCount    = 1;
    pb->rendering_ci.depthAttachmentFormat   = depth_format;
}

void pipeline_builder_set_shaders(PipelineBuilder* pb, VkShaderModule vert_shader, VkShaderModule frag_shader, const char* entry_name) {
    VkPipelineShaderStageCreateInfo vertex_stage_ci{};
    vertex_stage_ci.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage_ci.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage_ci.pName  = entry_name;
    vertex_stage_ci.module = vert_shader;

    VkPipelineShaderStageCreateInfo frag_stage_ci{};
    frag_stage_ci.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_ci.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_ci.pName  = entry_name;
    frag_stage_ci.module = frag_shader;

    pb->shader_stages.clear();
    pb->shader_stages.push_back(vertex_stage_ci);
    pb->shader_stages.push_back(frag_stage_ci);
}

void pipeline_builder_set_topology(PipelineBuilder* pb, VkPrimitiveTopology topology) {
    pb->input_assembly_state                        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    pb->input_assembly_state.topology               = topology;
    pb->input_assembly_state.primitiveRestartEnable = VK_FALSE;
}

void pipeline_builder_set_raster_state(PipelineBuilder* pb, VkCullModeFlags cull_mode, VkFrontFace front_face, VkPolygonMode poly_mode) {

    pb->rasterization_state                         = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    pb->rasterization_state.cullMode                = cull_mode;
    pb->rasterization_state.frontFace               = front_face;
    pb->rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    pb->rasterization_state.polygonMode             = poly_mode;
    pb->rasterization_state.lineWidth               = 1.f;
}

void pipeline_builder_set_multisampling(PipelineBuilder* pb, VkSampleCountFlagBits samples) {
    pb->multisample_state                       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pb->multisample_state.rasterizationSamples  = samples;
    pb->multisample_state.sampleShadingEnable   = VK_FALSE;
    pb->multisample_state.minSampleShading      = 1.0f;
    pb->multisample_state.pSampleMask           = nullptr;
    pb->multisample_state.alphaToCoverageEnable = VK_FALSE;
    pb->multisample_state.alphaToOneEnable      = VK_FALSE;
}

void pipeline_builder_set_depth_state(PipelineBuilder* pb, bool depth_test_enabled, bool write_enabled, VkCompareOp compare_op) {
    pb->depth_stencil_state                       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    pb->depth_stencil_state.depthTestEnable       = depth_test_enabled;
    pb->depth_stencil_state.depthWriteEnable      = write_enabled;
    pb->depth_stencil_state.depthCompareOp        = compare_op;
    pb->depth_stencil_state.stencilTestEnable     = VK_FALSE;
    pb->depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    pb->depth_stencil_state.front                 = {};
    pb->depth_stencil_state.back                  = {};
    pb->depth_stencil_state.minDepthBounds        = 0.f;
    pb->depth_stencil_state.maxDepthBounds        = 1.f;
}

void pipeline_builder_set_layout(PipelineBuilder* pb, std::span<VkDescriptorSetLayout> desc_set_layouts,
                                 std::span<VkPushConstantRange> push_constant_ranges, VkPipelineLayoutCreateFlags flags) {
    pb->pipeline_layout_ci                        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pb->pipeline_layout_ci.flags                  = flags;
    pb->pipeline_layout_ci.pSetLayouts            = desc_set_layouts.data();
    pb->pipeline_layout_ci.setLayoutCount         = desc_set_layouts.size();
    pb->pipeline_layout_ci.pPushConstantRanges    = push_constant_ranges.data();
    pb->pipeline_layout_ci.pushConstantRangeCount = push_constant_ranges.size();
}

void disable_pipeline_blending(PipelineBuilder* pb) {
    pb->color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pb->color_blend_attachment.blendEnable = VK_FALSE;
}

void set_pipeline_blending_additive(PipelineBuilder* pb) {
    pb->color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pb->color_blend_attachment.blendEnable         = VK_TRUE;
    pb->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pb->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    pb->color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    pb->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pb->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pb->color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
}

void set_pipeline_blending_alpha(PipelineBuilder* pb) {
    pb->color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pb->color_blend_attachment.blendEnable         = VK_TRUE;
    pb->color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    pb->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pb->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pb->color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    pb->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pb->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
}

void pipeline_builder_set_blending(PipelineBuilder* pb, BlendMode blend_mode) {
    switch (blend_mode) {
    case BlendMode::additive: {
        set_pipeline_blending_additive(pb);
        break;
    }
    case BlendMode::alpha: {
        set_pipeline_blending_alpha(pb);
        break;
    }
    default: {
        disable_pipeline_blending(pb);
        break;
    }
    }
}
