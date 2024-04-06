#include "vk_descriptor.h"
#include <vector>
#include <vulkan/vulkan_core.h>
void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {

  VkDescriptorSetLayoutBinding layout_binding{};
  layout_binding.binding = binding;
  layout_binding.descriptorType = type;
  layout_binding.descriptorCount = 1;

  _bindings.push_back(layout_binding);
}

void DescriptorLayoutBuilder::clear() { _bindings.clear(); }

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shader_stages) {

  for (auto& binding : _bindings) {
    binding.stageFlags |= shader_stages;
  }

  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;
  info.pBindings = _bindings.data();
  info.bindingCount = static_cast<uint32_t>(_bindings.size());
  info.flags = 0;

  VkDescriptorSetLayout set{};

  VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

  return set;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
  VkDescriptorBufferInfo& info =
      _buffer_infos.emplace_back(VkDescriptorBufferInfo{.buffer = buffer, .offset = offset, .range = size});

  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = &info;

  _writes.push_back(write);
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                                   VkDescriptorType type) {
  VkDescriptorImageInfo& info =
      _image_infos.emplace_back(VkDescriptorImageInfo{.sampler = sampler, .imageView = image, .imageLayout = layout});

  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it. see update_set()
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = &info;

  _writes.push_back(write);
}

void DescriptorWriter::clear() {
  _image_infos.clear();
  _writes.clear();
  _buffer_infos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set) {
  for (VkWriteDescriptorSet& write : _writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(device, _writes.size(), _writes.data(), 0, nullptr);
}

void DescriptorAllocator::create(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_size_ratios) {
  _ratios.clear();

  std::vector<VkDescriptorPoolSize> sizes;
  for (PoolSizeRatio ratio : pool_size_ratios) {
    _ratios.push_back(ratio);
  }

  VkDescriptorPool new_pool = create_pool(device, max_sets, pool_size_ratios);
  _ready_pools.push_back(new_pool);
  // set a bigger size for the next pool if few run out of space
  _sets_per_pool = max_sets * 1.5;
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
  VkDescriptorPool pool = get_pool(device);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.pSetLayouts = &layout;
  alloc_info.descriptorSetCount = 1;
  alloc_info.descriptorPool = pool;

  VkDescriptorSet new_set;
  VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &new_set);
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    _full_pools.push_back(pool);

    pool = get_pool(device);
    alloc_info.descriptorPool = pool;

    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &new_set););
  }

  _ready_pools.push_back(pool);
  return new_set;
}

void DescriptorAllocator::clear_pools(VkDevice device) {
  for (auto p : _ready_pools) {
    vkResetDescriptorPool(device, p, 0);
  }
  for (auto p : _full_pools) {
    vkResetDescriptorPool(device, p, 0);
    _ready_pools.push_back(p);
  }
  _full_pools.clear();
}

void DescriptorAllocator::destroy_pools(VkDevice device) {
  for (auto p : _ready_pools) {
    vkDestroyDescriptorPool(device, p, 0);
  }
  _ready_pools.clear();
  for (auto p : _full_pools) {
    vkDestroyDescriptorPool(device, p, 0);
  }
  _full_pools.clear();
}

// gets a ready pool or allocates another with more sets
VkDescriptorPool DescriptorAllocator::get_pool(VkDevice device) {
  VkDescriptorPool new_pool;

  if (_ready_pools.size() != 0) {
    new_pool = _ready_pools.back();
    _ready_pools.pop_back();
  } else {

    new_pool = create_pool(device, _sets_per_pool, _ratios);
    _sets_per_pool = _sets_per_pool * 1.5 > 4092 ? 4092 : _sets_per_pool * 1.5;
  }

  return new_pool;
}

VkDescriptorPool DescriptorAllocator::create_pool(VkDevice device, uint32_t max_sets,
                                                  std::span<PoolSizeRatio> pool_size_ratios) {

  std::vector<VkDescriptorPoolSize> sizes;
  for (PoolSizeRatio& ratio : pool_size_ratios) {
    sizes.push_back({
        .type = ratio.type,
        .descriptorCount = max_sets * ratio.desc_per_set,
    });
  }

  VkDescriptorPoolCreateInfo pool_ci{};
  pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_ci.maxSets = max_sets;
  pool_ci.pPoolSizes = sizes.data();
  pool_ci.poolSizeCount = sizes.size();
  // allows allocation past maxSets
  // pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_SETS_BIT_NV;

  VkDescriptorPool pool;
  VK_CHECK(vkCreateDescriptorPool(device, &pool_ci, nullptr, &pool));
  return pool;
}
