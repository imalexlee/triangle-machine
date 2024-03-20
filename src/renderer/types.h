#pragma once

#include <GLFW/glfw3.h>
#include <fmt/base.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(x)                                                                                                    \
  do {                                                                                                                 \
    VkResult err = x;                                                                                                  \
    if (err) {                                                                                                         \
      fmt::println("Detected Vulkan error: {}", string_VkResult(err));                                                 \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)
