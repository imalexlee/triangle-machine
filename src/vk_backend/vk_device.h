#pragma once

#include <cstdint>
#include <vk_backend/vk_types.h>

struct DeviceQueues {
    VkQueue  graphics;
    VkQueue  present;
    VkQueue  compute;
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    uint32_t compute_family_index;
};

struct DeviceContext {
    DeviceQueues     queues;
    VkDevice         logical_device;
    VkPhysicalDevice physical_device;
    uint32_t         raster_samples;
};

/**
 * @brief Initializes context for Vulkan devices, queues, and hadware capabilities
 *
 * @param device_ctx  DeviceContext to init
 * @param instance    Vulkan Instance used to check for physical devices
 * @param surface     Vulkan Surface used to check
 */
void init_device_context(DeviceContext* device_ctx, VkInstance instance, VkSurfaceKHR surface);

/**
 * @brief Frees Vulkan devices attached to this DeviceContext
 *
 * @param device_ctx DeviceContext to deinit
 */
void deinit_device_context(const DeviceContext* device_ctx);
