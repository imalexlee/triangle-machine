#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

struct PoolSizeRatio {
  VkDescriptorType type;
  uint32_t desc_per_set;
};

// 1. create the layout of a descriptor set
class DescriptorLayoutBuilder {
public:
  void add_binding(uint32_t binding, VkDescriptorType type);
  void clear();
  VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shader_stages);

private:
  std::vector<VkDescriptorSetLayoutBinding> _bindings;
};

// 2. create a pool and allow user to allocate a set with whatever layout
class DescriptorAllocator {
public:
  void create(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_size_ratios);
  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
  void clear_pools(VkDevice device);
  void destroy_pools(VkDevice device);

private:
  std::vector<VkDescriptorPool> _ready_pools;
  std::vector<VkDescriptorPool> _full_pools;
  std::vector<PoolSizeRatio> _ratios;
  uint32_t _sets_per_pool;

  VkDescriptorPool create_pool(VkDevice device, uint32_t max_sets,
                               std::span<PoolSizeRatio> pool_size_ratios);
  VkDescriptorPool get_pool(VkDevice device);
};

// 3. fill in an allocated set with actual data
class DescriptorWriter {
public:
  void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                   VkDescriptorType type);
  void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                    VkDescriptorType type);
  void clear();
  void update_set(VkDevice device, VkDescriptorSet set);

private:
  std::deque<VkDescriptorImageInfo> _image_infos;
  std::deque<VkDescriptorBufferInfo> _buffer_infos;
  std::vector<VkWriteDescriptorSet> _writes;
};
