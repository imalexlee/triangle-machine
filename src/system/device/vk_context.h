#pragma once
#include "vk_debug.h"
#include "vk_device.h"

struct VkContext {
    VkInstance          instance{};
    VkSurfaceKHR        surface{};
    VkPhysicalDevice    physical_device{};
    VkDevice            logical_device{};
    DeviceQueues        queues{};
    DeviceQueueFamilies queue_families{};
    Debugger            debugger{};
};

void vk_context_init(VkContext* vk_ctx, const struct Window* window);

void vk_context_deinit(VkContext* vk_ctx);
