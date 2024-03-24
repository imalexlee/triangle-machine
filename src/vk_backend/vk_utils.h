#pragma once

#include <deque>
#include <functional>

#define VK_CHECK(x)                                                                                                    \
  do {                                                                                                                 \
    VkResult err = x;                                                                                                  \
    if (err) {                                                                                                         \
      fmt::println("Detected Vulkan error: {}", string_VkResult(err));                                                 \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()>&& function) { deletors.push_back(function); }

  void flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();
    }

    deletors.clear();
  }
};
