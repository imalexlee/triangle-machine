#include "vk_descriptor.h"
#include <vector>
#include <vulkan/vulkan_core.h>

VkDescriptorPool create_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_size_ratios);
VkDescriptorPool desc_allocator_get_pool(DescriptorAllocator* desc_allocator, VkDevice device);

void desc_layout_builder_add_binding(DescriptorLayoutBuilder* layout_builder, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage,
                                     VkDescriptorBindingFlags binding_flags, uint32_t descriptor_count) {

    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding         = binding;
    layout_binding.descriptorType  = type;
    layout_binding.descriptorCount = descriptor_count;
    layout_binding.stageFlags      = stage;

    layout_builder->bindings.push_back(layout_binding);
    layout_builder->binding_flags.push_back(binding_flags);
}

void desc_layout_builder_clear(DescriptorLayoutBuilder* layout_builder) {
    layout_builder->bindings.clear();
    layout_builder->binding_flags.clear();
}

VkDescriptorSetLayout desc_layout_builder_create_layout(const DescriptorLayoutBuilder* layout_builder, VkDevice device) {

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{};
    binding_flags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags.bindingCount  = layout_builder->binding_flags.size();
    binding_flags.pBindingFlags = layout_builder->binding_flags.data();

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pBindings    = layout_builder->bindings.data();
    info.bindingCount = static_cast<uint32_t>(layout_builder->bindings.size());
    info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    info.pNext        = &binding_flags;

    VkDescriptorSetLayout set{};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void desc_writer_write_buffer_desc(DescriptorWriter* desc_writer, int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    const VkDescriptorBufferInfo& info =
        desc_writer->buffer_infos.emplace_back(VkDescriptorBufferInfo{.buffer = buffer, .offset = offset, .range = size});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    // left empty for now until we need to write it. see desc_writer_update_desc_set()
    write.dstSet          = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pBufferInfo     = &info;

    desc_writer->writes.push_back(write);
}

void desc_writer_write_image_desc(DescriptorWriter* desc_writer, int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                                  VkDescriptorType type, uint32_t array_idx) {
    VkDescriptorImageInfo& info =
        desc_writer->image_infos.emplace_back(VkDescriptorImageInfo{.sampler = sampler, .imageView = image, .imageLayout = layout});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    // left empty for now until we need to write it. see desc_writer_update_desc_set()
    write.dstSet          = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pImageInfo      = &info;
    write.dstArrayElement = array_idx;

    desc_writer->writes.push_back(write);
}

void desc_writer_clear(DescriptorWriter* desc_writer) {
    desc_writer->image_infos.clear();
    desc_writer->writes.clear();
    desc_writer->buffer_infos.clear();
}

void desc_writer_update_desc_set(DescriptorWriter* desc_writer, VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet& write : desc_writer->writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, desc_writer->writes.size(), desc_writer->writes.data(), 0, nullptr);
}

void desc_allocator_init(DescriptorAllocator* desc_allocator, VkDevice device, uint32_t init_set_count, std::span<PoolSizeRatio> pool_size_ratios) {
    desc_allocator->ratios.clear();

    std::vector<VkDescriptorPoolSize> sizes;
    for (PoolSizeRatio ratio : pool_size_ratios) {
        desc_allocator->ratios.push_back(ratio);
    }

    const VkDescriptorPool new_pool = create_pool(device, init_set_count, pool_size_ratios);
    desc_allocator->ready_pools.push_back(new_pool);
    // set a bigger size for the next pool if we run out of space
    desc_allocator->sets_per_pool = init_set_count * 1.5;
}

VkDescriptorSet desc_allocator_allocate_desc_set(DescriptorAllocator* desc_allocator, VkDevice device, VkDescriptorSetLayout layout,
                                                 uint32_t variable_desc_count) {
    VkDescriptorPool pool = desc_allocator_get_pool(desc_allocator, device);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pSetLayouts        = &layout;
    alloc_info.descriptorSetCount = 1;
    alloc_info.descriptorPool     = pool;

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_info{};
    variable_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    variable_info.descriptorSetCount = 1;
    variable_info.pDescriptorCounts  = &variable_desc_count;
    variable_info.descriptorSetCount = 1;

    if (variable_desc_count > 0) {
        alloc_info.pNext = &variable_info;
    }

    VkDescriptorSet new_set;
    VkResult        result = vkAllocateDescriptorSets(device, &alloc_info, &new_set);
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        desc_allocator->full_pools.push_back(pool);

        pool = desc_allocator_get_pool(desc_allocator, device);

        alloc_info.descriptorPool = pool;

        VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &new_set););
    }
    desc_allocator->ready_pools.push_back(pool);
    return new_set;
}

void desc_allocator_reset_desc_pools(DescriptorAllocator* desc_allocator, VkDevice device) {
    for (auto p : desc_allocator->ready_pools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : desc_allocator->full_pools) {
        vkResetDescriptorPool(device, p, 0);
        desc_allocator->ready_pools.push_back(p);
    }
    desc_allocator->full_pools.clear();
}

void desc_allocator_deinit(DescriptorAllocator* desc_allocator, VkDevice device) {
    for (const auto p : desc_allocator->ready_pools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    desc_allocator->ready_pools.clear();
    for (const auto p : desc_allocator->full_pools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    desc_allocator->full_pools.clear();
}

// gets a ready pool or allocates another with more sets
VkDescriptorPool desc_allocator_get_pool(DescriptorAllocator* desc_allocator, VkDevice device) {

    VkDescriptorPool new_pool;
    if (!desc_allocator->ready_pools.empty()) {
        new_pool = desc_allocator->ready_pools.back();
        desc_allocator->ready_pools.pop_back();
    } else {
        new_pool                      = create_pool(device, desc_allocator->sets_per_pool, desc_allocator->ratios);
        desc_allocator->sets_per_pool = desc_allocator->sets_per_pool * 1.5;
        if (desc_allocator->sets_per_pool > 4092) {
            desc_allocator->sets_per_pool = 4092;
        }
    }

    return new_pool;
}

VkDescriptorPool create_pool(VkDevice device, uint32_t max_sets, std::span<PoolSizeRatio> pool_size_ratios) {

    std::vector<VkDescriptorPoolSize> sizes;
    for (const PoolSizeRatio& ratio : pool_size_ratios) {
        sizes.push_back({
            .type            = ratio.type,
            .descriptorCount = max_sets * ratio.desc_per_set,
        });
    }

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.maxSets       = max_sets;
    pool_ci.pPoolSizes    = sizes.data();
    pool_ci.poolSizeCount = sizes.size();
    pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_ci, nullptr, &pool));
    return pool;
}