#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace vk_opts {
    inline constexpr bool     msaa_enabled         = true;
    inline constexpr uint32_t desired_msaa_samples = 4;
    inline constexpr uint64_t timeout_dur          = 1'000'000'000;

    // will default to VK_PRESENT_MODE_FIFO_KHR if desired cannot be found
    inline constexpr VkPresentModeKHR desired_present_mode = VK_PRESENT_MODE_FIFO_KHR;

#ifdef NDEBUG
    inline constexpr bool validation_enabled = false;
#else
    inline constexpr bool validation_enabled = true;
#endif // NDEBUG

} // namespace
  // vk_opts
