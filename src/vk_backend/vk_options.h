#include <cstdint>
namespace vk_opts {
  inline constexpr bool msaa_enabled = true;
  inline constexpr uint64_t timeout_dur = 1'000'000'000;

#ifdef NDEBUG
  inline constexpr bool validation_enabled = false;
#else
  inline constexpr bool validation_enabled = true;
#endif // NDEBUG

} // namespace vk_opts
