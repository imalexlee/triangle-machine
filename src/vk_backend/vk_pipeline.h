#pragma once

#include <span>
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

enum BlendMode {
    additive,
    alpha,
    none,
};

struct PipelineInfo {
    VkPipeline       pipeline;
    VkPipelineLayout pipeline_layout;
};

struct PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    VkPipelineInputAssemblyStateCreateInfo       input_assembly_state;
    VkPipelineRasterizationStateCreateInfo       rasterization_state;
    VkPipelineMultisampleStateCreateInfo         multisample_state;
    VkPipelineDepthStencilStateCreateInfo        depth_stencil_state;
    VkPipelineColorBlendAttachmentState          color_blend_attachment;
    VkPipelineLayoutCreateInfo                   pipeline_layout_ci;
    VkPipelineRenderingCreateInfo                rendering_ci;
    VkFormat                                     color_attachment_format;
};

/**
 * @brief Creates a Vulkan pipeline and pipeline layout based on the state of the builder
 *
 * @param pb	  The PipelineBuilder which is storing the state to create the pipeline
 * @param device  The device to be associated with this pipeline and layout
 * @return	  A graphics pipeline and pipeline layout
 */
[[nodiscard]] PipelineInfo build_pipeline(PipelineBuilder* pb, VkDevice device);

/**
 * @brief Sets the vertex and fragment shaders for the desired pipeline
 *
 * @param pb	      The PipelineBuilder to store these shaders
 * @param vert_shader The vertex shader
 * @param frag_shader The Fragment shader
 * @param entry_name  The entry point within the both shaders
 */
void set_pipeline_shaders(PipelineBuilder* pb,
                          VkShaderModule   vert_shader,
                          VkShaderModule   frag_shader,
                          const char*      entry_name = "main");

/**
 * @brief Sets the primitive topology for the desired pipeline
 *
 * @param pb	    The PipelineBuilder to store the topology selection
 * @param topology  The primitive topology for data going into this pipeline
 */
void set_pipeline_topology(PipelineBuilder* pb, VkPrimitiveTopology topology);

/**
 * @brief Sets the rasterization state for the desired pipeline
 *
 * @param pb	      The PipelineBuilder to store the rasterization state
 * @param cull_mode   The culling mode of the rasterizer
 * @param front_face  What orientation to read as the front face of vertex data
 * @param poly_mode   The polygon mode of vertex data
 */
void set_pipeline_raster_state(PipelineBuilder* pb,
                               VkCullModeFlags  cull_mode,
                               VkFrontFace      front_face,
                               VkPolygonMode    poly_mode);

/**
 * @brief Sets the multisampling state for the desired pipeline
 *
 * @param pb	    The PipelineBuilder to store the multisampling state
 * @param samples   The number of samples to gather during multisampling
 */
void set_pipeline_multisampling(PipelineBuilder* pb, VkSampleCountFlagBits samples);

/**
 * @brief Sets the depth and depth testing state for the desired pipeline
 *
 * @param pb		      The PipelineBuilder to store the depth state
 * @param depth_test_enabled  Whether or not to enable depth testing
 * @param write_enabled	      Whether or not to enable writes to depth buffers
 * @param compare_op	      What comparison operation to use when comparing depth
 */
void set_pipeline_depth_state(PipelineBuilder* pb,
                              bool             depth_test_enabled,
                              bool             write_enabled,
                              VkCompareOp      compare_op);

/**
 * @brief Sets the dynamic rendering state for the desired pipeline
 *
 * @param pb		The PipelineBuilder to store the rendering state
 * @param color_format	The format of the color attachment
 * @param depth_format	The format of the depth attachment
 */
void set_pipeline_render_state(PipelineBuilder* pb, VkFormat color_format, VkFormat depth_format);

/**
 * @brief Sets the pipeline layout info for the desired pipeline
 *
 * @param pb			The PipelineBuilder to store the pipeline layout info
 * @param desc_set_layouts	The descriptor set layouts for objects using this pipeline
 * @param push_constant_ranges	The push constant ranges for objects using this pipeline
 * @param flags			Flags to configure pipeline layout creation
 */
void set_pipeline_layout(PipelineBuilder*                 pb,
                         std::span<VkDescriptorSetLayout> desc_set_layouts,
                         std::span<VkPushConstantRange>   push_constant_ranges,
                         VkPipelineLayoutCreateFlags      flags);

/**
 * @brief Sets the color blending mode for the desired pipeline
 *
 * @param pb	      The PipelineBuilder to store the blending mode info
 * @param blend_mode  The desired blend mode
 */
void set_pipeline_blending(PipelineBuilder* pb, BlendMode blend_mode);
