#pragma once

#include "vk_backend/vk_device.h"
#include "vk_mem_alloc.h"

VmaAllocator create_allocator(VkInstance instance, DeviceContext& device_context);
