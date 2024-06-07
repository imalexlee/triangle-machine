#pragma once

#include <span>
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct PipelineInfo {
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
};

class PipelineBuilder {
public:
  PipelineBuilder() { clear(); }

  PipelineInfo build_pipeline(VkDevice device);
  void set_shader_stages(VkShaderModule vert_shader, VkShaderModule frag_shader,
                         const char* entry_name = "mai"
                                                  "n");
  void set_input_assembly(VkPrimitiveTopology topology);
  void set_raster_culling(VkCullModeFlags cull_mode, VkFrontFace front_face);
  void set_raster_poly_mode(VkPolygonMode poly_mode);
  void set_multisample_state(VkSampleCountFlagBits samples);
  void set_depth_stencil_state(bool depth_test_enabled, bool write_enabled, VkCompareOp compare_op);
  void set_render_info(VkFormat color_format, VkFormat depth_format);
  void set_layout(std::span<VkDescriptorSetLayout> desc_set_layouts,
                  std::span<VkPushConstantRange> push_constant_ranges,
                  VkPipelineLayoutCreateFlags flags);
  void enable_blending_additive();
  void enable_blending_alphablend();
  void disable_blending();
  void clear();

private:
  std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;
  VkPipelineInputAssemblyStateCreateInfo _input_assembly_state;
  VkPipelineRasterizationStateCreateInfo _rasterization_state;
  VkPipelineMultisampleStateCreateInfo _multisample_state;
  VkPipelineDepthStencilStateCreateInfo _depth_stencil_state;
  VkPipelineColorBlendAttachmentState _color_blend_attachment;
  VkPipelineLayoutCreateInfo _pipeline_layout_ci;
  VkPipelineRenderingCreateInfo _rendering_ci;
  VkFormat _color_attachment_format;
};
