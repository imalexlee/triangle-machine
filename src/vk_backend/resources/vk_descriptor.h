#pragma once

#include "vk_backend/vk_device.h"
#include <span>
#include <vk_backend/vk_types.h>

class DescriptorContext {
public:
  VkDescriptorPool pool;

  void create(DeviceContext& device_context, std::span<VkDescriptorPoolSize> pool_sizes);

private:
};
