#pragma once

#include "system/device/vk_types.h"
#include <cstdint>

struct DeviceQueues {
    VkQueue graphics;
    VkQueue present;
    VkQueue compute;
    // uint32_t graphics_family_index{};
    // uint32_t present_family_index{};
    // uint32_t compute_family_index{};
};

struct DeviceQueueFamilies {
    uint32_t graphics{};
    uint32_t present{};
    uint32_t compute{};
};

// struct DeviceContext {
//     DeviceQueues     queues{};
//     VkDevice         logical_device;
//     VkPhysicalDevice physical_device;
//     uint32_t         msaa_samples{1};
// };
//

/**
 * @brief Initializes context for Vulkan devices, queues, and hardware capabilities
 *
 * @param device_ctx  DeviceContext to init
 * @param instance    Vulkan Instance used to check for physical devices
 * @param surface     Vulkan Surface used to check
 */
// void device_ctx_init(DeviceContext* device_ctx, VkInstance instance, VkSurfaceKHR surface);

/**
 * @brief Frees Vulkan devices attached to this DeviceContext
 *
 * @param device_ctx DeviceContext to deinit
 */
// void device_ctx_deinit(const DeviceContext* device_ctx);

[[nodiscard]] VkPhysicalDevice vk_physical_device_create(VkInstance instance);

[[nodiscard]] DeviceQueueFamilies vk_queue_families_get(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

[[nodiscard]] VkDevice vk_logical_device_create(VkPhysicalDevice physical_device, const DeviceQueueFamilies* queue_families);

[[nodiscard]] DeviceQueues vk_device_queues_create(VkDevice device, const DeviceQueueFamilies* queue_families);
