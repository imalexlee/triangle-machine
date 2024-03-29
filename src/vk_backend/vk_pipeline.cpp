#include "vk_pipeline.h"
#include <vulkan/vulkan_core.h>

VkPipeline PipelineBuilder::build_pipeline(VkDevice device) {
  VkPipeline new_pipeline;

  VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
  vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  std::array<VkDynamicState, 2> dynamic_states{VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
  VkPipelineDynamicStateCreateInfo dynamic_ci{};
  dynamic_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_ci.pDynamicStates = dynamic_states.data();
  dynamic_ci.dynamicStateCount = dynamic_states.size();

  VkPipelineViewportStateCreateInfo viewport_ci{};
  viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_ci.scissorCount = 1;
  viewport_ci.viewportCount = 1;

  VkGraphicsPipelineCreateInfo pipeline_ci{};
  pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_ci.pStages = _shader_stages.data();
  pipeline_ci.stageCount = _shader_stages.size();
  pipeline_ci.pVertexInputState = &vertex_input_ci;
  pipeline_ci.pInputAssemblyState = &_input_assembly_state;
  pipeline_ci.pDynamicState = &dynamic_ci;
  pipeline_ci.pViewportState = &viewport_ci;

  // TODO: rasterization state next
  // pipeline_ci.pRasterizationState = &_rasterization_state;

  VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_ci, nullptr, &new_pipeline));
  return new_pipeline;
}

void PipelineBuilder::set_shader_stages(VkShaderModule vert_shader, VkShaderModule frag_shader) {
  VkPipelineShaderStageCreateInfo vertex_stage_ci{};
  vertex_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage_ci.pName = "vertex shader";
  vertex_stage_ci.module = vert_shader;

  VkPipelineShaderStageCreateInfo frag_stage_ci{};
  frag_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_stage_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_stage_ci.pName = "fragment shader";
  frag_stage_ci.module = frag_shader;

  _shader_stages.clear();
  _shader_stages.push_back(vertex_stage_ci);
  _shader_stages.push_back(frag_stage_ci);
}

void PipelineBuilder::set_input_assembly(VkPrimitiveTopology topology) {
  _input_assembly_state.topology = topology;
  _input_assembly_state.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::clear() {
  _input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}
