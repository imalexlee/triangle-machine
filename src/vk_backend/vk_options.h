namespace vk_opts {
#ifdef NDEBUG
  inline constexpr bool validation_enabled = false;
#else
  inline constexpr bool validation_enabled = true;
#endif // NDEBUG

  inline constexpr bool msaa_enabled = true;

} // namespace vk_opts
