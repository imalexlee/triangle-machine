#pragma once

#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

class PipelineBuilder {
public:
  PipelineBuilder() { clear(); }

  VkPipeline build_pipeline(VkDevice device);
  void set_shader_stages(VkShaderModule vert_shader, VkShaderModule frag_shader);
  void set_input_assembly(VkPrimitiveTopology topology);
  void clear();

private:
  // try std::array
  std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;
  VkPipelineInputAssemblyStateCreateInfo _input_assembly_state;
  VkPipelineRasterizationStateCreateInfo _rasterization_state;
};
