#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Window {
    GLFWwindow* glfw_window{};
    const char* title;
    uint32_t    width;
    uint32_t    height;

    static inline std::vector<std::function<void(int, int, int, int)>> key_callbacks;
    static inline std::vector<std::function<void(double, double)>>     cursor_callbacks;
    static inline std::vector<std::function<void(int, int)>>           resize_callbacks;
};

void init_window(Window* window, uint32_t width, uint32_t height, const char* title);

void deinit_window(const Window* window);

[[nodiscard]] VkSurfaceKHR get_vulkan_surface(const Window* window, VkInstance instance);

void register_key_callback(Window* window, std::function<void(int, int, int, int)>&& fn_ptr);

void register_cursor_callback(Window* window, std::function<void(double, double)>&& fn_ptr);
