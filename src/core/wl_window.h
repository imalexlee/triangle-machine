#pragma once

#include <vk_backend/vk_backend.h>
#include <vulkan/vulkan_core.h>

inline void init_wayland_client(const char* app_name);

[[nodiscard]] inline VkSurfaceKHR get_wayland_surface(VkInstance instance);

inline void deinit_wayland_client();
