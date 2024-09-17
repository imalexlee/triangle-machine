#pragma once

#include <deque>
#include <functional>
#include <vulkan/generated/vk_enum_string_helper.h>

#define VK_CHECK(x)                                                                                \
    do {                                                                                           \
        VkResult err = x;                                                                          \
        if (err) {                                                                                 \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err));                       \
            abort();                                                                               \
        }                                                                                          \
    } while (0)

// allows the pushing and flushing of consistently updating data and long living data
class DeletionQueue {
  public:
    void push_volatile(std::function<void()>&& function) { _volatile_deletors.push_back(function); }
    void push_persistant(std::function<void()>&& function) {
        _persistant_deletors.push_back(function);
    }

    // flushes everything
    void flush() {
        for (auto it = _volatile_deletors.rbegin(); it != _volatile_deletors.rend(); it++) {
            (*it)();
        }
        _volatile_deletors.clear();
        for (auto it = _persistant_deletors.rbegin(); it != _persistant_deletors.rend(); it++) {
            (*it)();
        }
        _persistant_deletors.clear();
    }

    void flush_persistant() {
        for (auto it = _persistant_deletors.rbegin(); it != _persistant_deletors.rend(); it++) {
            (*it)();
        }
        _persistant_deletors.clear();
    }

    void flush_volatile() {
        for (auto it = _volatile_deletors.rbegin(); it != _volatile_deletors.rend(); it++) {
            (*it)();
        }
        _volatile_deletors.clear();
    }

  private:
    std::deque<std::function<void()>> _volatile_deletors;
    std::deque<std::function<void()>> _persistant_deletors;
};
