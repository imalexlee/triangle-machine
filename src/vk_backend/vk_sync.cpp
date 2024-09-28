#include "vk_sync.h"
#include "vk_backend/resources/vk_image.h"

VkSemaphore vk_semaphore_create(VkDevice device, VkSemaphoreType type, uint64_t initial_timeline_value) {
    VkSemaphore               semaphore;
    VkSemaphoreCreateInfo     semaphore_ci{};
    VkSemaphoreTypeCreateInfo semaphore_type_ci{};

    semaphore_type_ci.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphore_type_ci.semaphoreType = type;
    semaphore_type_ci.initialValue  = type & VK_SEMAPHORE_TYPE_BINARY ? 0 : initial_timeline_value;

    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_ci.pNext = &semaphore_type_ci;
    semaphore_ci.flags = 0;

    VK_CHECK(vkCreateSemaphore(device, &semaphore_ci, nullptr, &semaphore));

    return semaphore;
}

VkFence vk_fence_create(VkDevice device, VkFenceCreateFlags flags) {
    VkFence           fence;
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = flags;
    fence_ci.pNext = nullptr;

    VK_CHECK(vkCreateFence(device, &fence_ci, nullptr, &fence));

    return fence;
}

VkSemaphoreSubmitInfo vk_semaphore_submit_info_create(VkSemaphore semaphore, VkPipelineStageFlags2 stages, uint64_t timeline_value) {
    VkSemaphoreSubmitInfo semaphore_si{};
    semaphore_si.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphore_si.value       = timeline_value;
    semaphore_si.semaphore   = semaphore;
    semaphore_si.stageMask   = stages;
    semaphore_si.deviceIndex = 0;
    semaphore_si.pNext       = nullptr;

    return semaphore_si;
}

void vk_image_memory_barrier_insert(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout,
                                    uint32_t layer_count, VkPipelineStageFlags2 src_stages, VkPipelineStageFlags2 dst_stages) {

    VkImageMemoryBarrier2 image_mem_barrier{};
    image_mem_barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_mem_barrier.pNext         = nullptr;
    image_mem_barrier.image         = image;
    image_mem_barrier.srcStageMask  = src_stages;
    image_mem_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_mem_barrier.dstStageMask  = dst_stages;
    image_mem_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    image_mem_barrier.oldLayout     = current_layout;
    image_mem_barrier.newLayout     = new_layout;

    VkDependencyInfo dep_info{};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext = nullptr;

    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (image_mem_barrier.newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageSubresourceRange subresource_range = vk_image_subresource_range_create(aspect_flags, layer_count, 1);
    image_mem_barrier.subresourceRange        = subresource_range;

    dep_info.pImageMemoryBarriers    = &image_mem_barrier;
    dep_info.imageMemoryBarrierCount = 1;

    vkCmdPipelineBarrier2(cmd_buf, &dep_info);
}
