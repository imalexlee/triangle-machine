#include "vk_descriptor.h"
#include <vector>
#include <vulkan/vulkan_core.h>

VkDescriptorPool create_pool(VkDevice device, uint32_t max_sets,
                             std::span<PoolSizeRatio> pool_size_ratios);
VkDescriptorPool get_pool(DescriptorAllocator* desc_allocator, VkDevice device);

void add_layout_binding(DescriptorLayoutBuilder* layout_builder, uint32_t binding,
                        VkDescriptorType type) {
    VkDescriptorSetLayoutBinding layout_binding{};
    layout_binding.binding         = binding;
    layout_binding.descriptorType  = type;
    layout_binding.descriptorCount = 1;

    layout_builder->bindings.push_back(layout_binding);
}

void clear_layout_bindings(DescriptorLayoutBuilder* layout_builder) {
    layout_builder->bindings.clear();
}

VkDescriptorSetLayout build_set_layout(DescriptorLayoutBuilder* layout_builder, VkDevice device,
                                       VkShaderStageFlags shader_stages) {
    for (auto& binding : layout_builder->bindings) {
        binding.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext        = nullptr;
    info.pBindings    = layout_builder->bindings.data();
    info.bindingCount = static_cast<uint32_t>(layout_builder->bindings.size());
    info.flags        = 0;

    VkDescriptorSetLayout set{};
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void write_buffer_desc(DescriptorWriter* desc_writer, int binding, VkBuffer buffer, size_t size,
                       size_t offset, VkDescriptorType type) {
    const VkDescriptorBufferInfo& info = desc_writer->buffer_infos.emplace_back(
        VkDescriptorBufferInfo{.buffer = buffer, .offset = offset, .range = size});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    // left empty for now until we need to write it. see update_desc_set()
    write.dstSet          = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pBufferInfo     = &info;

    desc_writer->writes.push_back(write);
}

void write_image_desc(DescriptorWriter* desc_writer, int binding, VkImageView image,
                      VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo& info = desc_writer->image_infos.emplace_back(
        VkDescriptorImageInfo{.sampler = sampler, .imageView = image, .imageLayout = layout});

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    write.dstBinding = binding;
    // left empty for now until we need to write it. see update_desc_set()
    write.dstSet          = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType  = type;
    write.pImageInfo      = &info;

    desc_writer->writes.push_back(write);
}

void clear_desc_writer(DescriptorWriter* desc_writer) {
    desc_writer->image_infos.clear();
    desc_writer->writes.clear();
    desc_writer->buffer_infos.clear();
}

void update_desc_set(DescriptorWriter* desc_writer, VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet& write : desc_writer->writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, desc_writer->writes.size(), desc_writer->writes.data(), 0,
                           nullptr);
}

void init_desc_allocator(DescriptorAllocator* desc_allocator, VkDevice device,
                         uint32_t init_set_count, std::span<PoolSizeRatio> pool_size_ratios) {
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

VkDescriptorSet allocate_desc_set(DescriptorAllocator* desc_allocator, VkDevice device,
                                  VkDescriptorSetLayout layout) {
    VkDescriptorPool            pool = get_pool(desc_allocator, device);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pSetLayouts        = &layout;
    alloc_info.descriptorSetCount = 1;
    alloc_info.descriptorPool     = pool;

    VkDescriptorSet new_set;
    VkResult        result = vkAllocateDescriptorSets(device, &alloc_info, &new_set);
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        desc_allocator->full_pools.push_back(pool);

        pool                      = get_pool(desc_allocator, device);
        alloc_info.descriptorPool = pool;

        VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &new_set););
    }
    desc_allocator->ready_pools.push_back(pool);
    return new_set;
}

void reset_desc_pools(DescriptorAllocator* desc_allocator, VkDevice device) {
    for (auto p : desc_allocator->ready_pools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : desc_allocator->full_pools) {
        vkResetDescriptorPool(device, p, 0);
        desc_allocator->ready_pools.push_back(p);
    }
    desc_allocator->full_pools.clear();
}

void deinit_desc_allocator(DescriptorAllocator* desc_allocator, VkDevice device) {
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
VkDescriptorPool get_pool(DescriptorAllocator* desc_allocator, VkDevice device) {

    VkDescriptorPool new_pool;
    if (!desc_allocator->ready_pools.empty()) {
        new_pool = desc_allocator->ready_pools.back();
        desc_allocator->ready_pools.pop_back();
    } else {
        new_pool = create_pool(device, desc_allocator->sets_per_pool, desc_allocator->ratios);
        desc_allocator->sets_per_pool = desc_allocator->sets_per_pool * 1.5;
        if (desc_allocator->sets_per_pool > 4092) {
            desc_allocator->sets_per_pool = 4092;
        }
    }

    return new_pool;
}

VkDescriptorPool create_pool(VkDevice device, uint32_t max_sets,
                             std::span<PoolSizeRatio> pool_size_ratios) {

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

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &pool_ci, nullptr, &pool));
    return pool;
}
