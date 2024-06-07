#pragma once

#include <cstdint>
namespace vk_opts {
  inline constexpr bool msaa_enabled = false;
  inline constexpr uint64_t timeout_dur = 1'000'000'000;

  // total swapchain
  // images to
  // request. will
  // clamp to
  // physical bounds
  inline constexpr uint32_t frame_count = 3;

#ifdef NDEBUG
  inline constexpr bool validation_enabled = false;
#else
  inline constexpr bool validation_enabled = true;
#endif // NDEBUG

} // namespace
  // vk_opts
