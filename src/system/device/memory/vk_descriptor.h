#pragma once

#include <cstdint>
#include <span>
#include <system/device/vk_types.h>
#include <vector>
#include <vulkan/vulkan_core.h>

struct PoolSizeRatio {
    VkDescriptorType type;
    uint32_t         desc_per_set{};
};

// 1. create the layout of a descriptor set
struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorBindingFlags>     binding_flags;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

/**
 * @brief Appends a descriptor set layout binding to this layout builder
 *
 * @param layout_builder   The layout builder to append to
 * @param binding	       The index location of the descriptor set to bind to
 * @param type		       The type of descriptor
 * @param stage            The shader stage to connect this binding with
 * @param binding_flags    The binding flags for this layout binding
 * @param descriptor_count The number of descriptors with this type and stage
 */
void desc_layout_builder_add_binding(DescriptorLayoutBuilder* layout_builder, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage,
                                     VkDescriptorBindingFlags binding_flags = 0, uint32_t descriptor_count = 1);

/**
 * @brief Clears the current layout bindings for this layout builder
 *
 * @param layout_builder The layout builder to clear
 */
void desc_layout_builder_clear(DescriptorLayoutBuilder* layout_builder);

/**
 * @brief Builds a descriptor set layout based on the bindings in a layout builder
 *
 * @param layout_builder  The layout builder to build the set from
 * @param device	  The device associated with this set layout
 * @return		  The bound descriptor set layout
 */
[[nodiscard]] VkDescriptorSetLayout desc_layout_builder_create_layout(const DescriptorLayoutBuilder* layout_builder, VkDevice device);

// 2. create a pool and allow user to allocate a set with whatever layout
struct DescriptorAllocator {
    std::vector<VkDescriptorPool> ready_pools;
    std::vector<VkDescriptorPool> full_pools;
    std::vector<PoolSizeRatio>    ratios;
    uint32_t                      sets_per_pool;
};

/**
 * @brief Initializes an allocator that can dynamically allocate descriptor sets
 *
 * @param desc_allocator	    The DescriptorAllocator to initialize
 * @param device	    The device to allocate descriptor pools from
 * @param init_set_count    The initial amount of descriptor sets to be allocated in the pool
 * @param pool_size_ratios  The type and amount of descriptors to reserve space for in each set
 */
void desc_allocator_init(DescriptorAllocator* desc_allocator, VkDevice device, uint32_t init_set_count, std::span<PoolSizeRatio> pool_size_ratios);

/**
 * @brief Trys to allocate a descriptor set from an existing pool or creates a new pool if no space
 *
 * @param desc_allocator      The allocator to allocate with
 * @param device	          The device to allocate from
 * @param layout	          The layout of the descriptor set to allocate
 * @param variable_desc_count If the set was created with a variable binding, set this count
 *                            to represent how many actual descriptors to allocate here
 * @return		              An allocated descriptor set with the desired layout
 */
[[nodiscard]] VkDescriptorSet desc_allocator_allocate_desc_set(DescriptorAllocator* desc_allocator, VkDevice device, VkDescriptorSetLayout layout,
                                                               uint32_t variable_desc_count = 0);

/**
 * @brief Resets all descriptor pools and marks all previously allocated pools as ready-to-use
 *
 * @param desc_allocator  The allocator to use to reset pools
 * @param device	  The device to reset the pools for
 */
void desc_allocator_reset_desc_pools(DescriptorAllocator* desc_allocator, VkDevice device);

/**
 * @brief Destroys all descriptor pools associated with this descriptor allocator
 *
 * @param desc_allocator  The DescriptorAllocator to deinitialize
 * @param device	  The device that the pools were allocated from
 */
void desc_allocator_deinit(DescriptorAllocator* desc_allocator, VkDevice device);

// 3. fill in an allocated set with actual data
struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo>                        image_infos;
    std::deque<VkDescriptorBufferInfo>                       buffer_infos;
    std::deque<VkWriteDescriptorSetAccelerationStructureKHR> accel_struct_writes;

    std::vector<VkWriteDescriptorSet> writes;
};

/**
 * @brief Appends an image descriptor to a list of descriptors in which to eventually write to a set
 *
 * @param desc_writer The descriptor writer used to write this image view
 * @param binding     The index location to write this image view to in the set
 * @param image	      The image view to write
 * @param sampler     The sampler to associate with this image view
 * @param layout      The layout of this image
 * @param type	      The type of descriptor
 * @param array_idx   The index of descriptor to write at this binding
 */
void desc_writer_write_image_desc(DescriptorWriter* desc_writer, int binding, VkImageView image, VkSampler sampler, VkImageLayout layout,
                                  VkDescriptorType type, uint32_t array_idx = 0);

/**
 * @brief Appends a buffer descriptor to a list of descriptors in which to eventually write to a set
 *
 * @param desc_writer The descriptor writer used to write this buffer
 * @param binding     The index location to write this buffer to in the set
 * @param buffer      The buffer to write
 * @param size        The size of this buffer in bytes to use starting from the offset
 * @param offset      The starting offset position to read this buffer from
 * @param type        The type of descriptor
 */
void desc_writer_write_buffer_desc(DescriptorWriter* desc_writer, int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

void desc_writer_write_accel_struct_desc(DescriptorWriter* desc_writer, int binding, const VkAccelerationStructureKHR* accel_struct,
                                         VkDescriptorType type);

/**
 * @brief Writes out saved image and/or buffer descriptors in our descriptor writer to an allocated
 * set
 *
 * @param desc_writer The DescriptorWriter which is storing our pending buffers/image descriptors
 * @param device      The device in which our descriptor set is allocated from
 * @param set	      The allocated descriptor set to fill in
 */
void desc_writer_update_desc_set(DescriptorWriter* desc_writer, VkDevice device, VkDescriptorSet set);

/**
 * @brief Clears the state of this DescriptorWriter
 *
 * @param desc_writer the writer to clear
 */
void desc_writer_clear(DescriptorWriter* desc_writer);
