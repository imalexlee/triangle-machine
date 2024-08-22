#include "vk_pipeline.h"
#include "vk_backend/vk_utils.h"
#include <fmt/base.h>
#include <vulkan/vulkan_core.h>

PipelineInfo PipelineBuilder::build_pipeline(VkDevice device) {
    VkPipeline new_pipeline;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_ci.vertexBindingDescriptionCount = 0;
    vertex_input_ci.vertexAttributeDescriptionCount = 0;

    std::array<VkDynamicState, 2> dynamic_states{VK_DYNAMIC_STATE_SCISSOR,
                                                 VK_DYNAMIC_STATE_VIEWPORT};

    VkPipelineDynamicStateCreateInfo dynamic_ci{};
    dynamic_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_ci.pDynamicStates = dynamic_states.data();
    dynamic_ci.dynamicStateCount = dynamic_states.size();

    // not setting the viewport and scissor as its dynamic state
    VkPipelineViewportStateCreateInfo viewport_ci{};
    viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_ci.scissorCount = 1;
    viewport_ci.viewportCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.pNext = nullptr;
    color_blend_state.logicOpEnable = VK_FALSE;
    color_blend_state.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &_color_blend_attachment;

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(device, &_pipeline_layout_ci, nullptr, &pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // for dynamic rendering
    pipeline_ci.pNext = &_rendering_ci;
    pipeline_ci.pStages = _shader_stages.data();
    pipeline_ci.stageCount = _shader_stages.size();
    pipeline_ci.pVertexInputState = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &_input_assembly_state;
    pipeline_ci.pDynamicState = &dynamic_ci;
    pipeline_ci.pViewportState = &viewport_ci;
    pipeline_ci.pRasterizationState = &_rasterization_state;
    pipeline_ci.pMultisampleState = &_multisample_state;
    pipeline_ci.pDepthStencilState = &_depth_stencil_state;
    pipeline_ci.pColorBlendState = &color_blend_state;
    pipeline_ci.layout = pipeline_layout;

    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_ci, nullptr, &new_pipeline));
    return {
        .pipeline = new_pipeline,
        .pipeline_layout = pipeline_layout,
    };
}

void PipelineBuilder::set_render_info(VkFormat color_format, VkFormat depth_format) {
    _color_attachment_format = color_format;
    _rendering_ci.pColorAttachmentFormats = &_color_attachment_format;
    _rendering_ci.colorAttachmentCount = 1;
    _rendering_ci.depthAttachmentFormat = depth_format;
}

void PipelineBuilder::set_shader_stages(VkShaderModule vert_shader, VkShaderModule frag_shader,
                                        const char* entry_name) {
    VkPipelineShaderStageCreateInfo vertex_stage_ci{};
    vertex_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage_ci.pName = entry_name;
    vertex_stage_ci.module = vert_shader;

    VkPipelineShaderStageCreateInfo frag_stage_ci{};
    frag_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_ci.pName = entry_name;
    frag_stage_ci.module = frag_shader;

    _shader_stages.clear();
    _shader_stages.push_back(vertex_stage_ci);
    _shader_stages.push_back(frag_stage_ci);
}

void PipelineBuilder::set_input_assembly(VkPrimitiveTopology topology) {
    _input_assembly_state.topology = topology;
    _input_assembly_state.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_raster_culling(VkCullModeFlags cull_mode, VkFrontFace front_face) {
    _rasterization_state.cullMode = cull_mode;
    _rasterization_state.frontFace = front_face;
    _rasterization_state.rasterizerDiscardEnable = VK_FALSE;
}
void PipelineBuilder::set_raster_poly_mode(VkPolygonMode poly_mode) {
    _rasterization_state.polygonMode = poly_mode;
    _rasterization_state.lineWidth = 1.f;
}

void PipelineBuilder::set_multisample_state(VkSampleCountFlagBits samples) {
    _multisample_state.rasterizationSamples = samples;
    _multisample_state.sampleShadingEnable = VK_FALSE;
    _multisample_state.minSampleShading = 1.0f;
    _multisample_state.pSampleMask = nullptr;
    _multisample_state.alphaToCoverageEnable = VK_FALSE;
    _multisample_state.alphaToOneEnable = VK_FALSE;
}
void PipelineBuilder::set_depth_stencil_state(bool depth_test_enabled, bool write_enabled,
                                              VkCompareOp compare_op) {
    _depth_stencil_state.depthTestEnable = depth_test_enabled;
    _depth_stencil_state.depthWriteEnable = write_enabled;
    _depth_stencil_state.depthCompareOp = compare_op;
    _depth_stencil_state.stencilTestEnable = VK_FALSE;
    _depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    _depth_stencil_state.front = {};
    _depth_stencil_state.back = {};
    _depth_stencil_state.minDepthBounds = 0.f;
    _depth_stencil_state.maxDepthBounds = 1.f;
}

void PipelineBuilder::disable_blending() {
    _color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::enable_blending_additive() {
    _color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend() {
    _color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    _color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::set_layout(std::span<VkDescriptorSetLayout> desc_set_layouts,
                                 std::span<VkPushConstantRange> push_constant_ranges,
                                 VkPipelineLayoutCreateFlags flags) {
    _pipeline_layout_ci.flags = flags;
    _pipeline_layout_ci.pSetLayouts = desc_set_layouts.data();
    _pipeline_layout_ci.setLayoutCount = desc_set_layouts.size();
    _pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
    _pipeline_layout_ci.pushConstantRangeCount = push_constant_ranges.size();
}

void PipelineBuilder::clear() {
    _input_assembly_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    _rasterization_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    _multisample_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    _depth_stencil_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    _pipeline_layout_ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    _rendering_ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
}
