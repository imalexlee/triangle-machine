#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Window {
    GLFWwindow*            glfw_window{};
    const char*            title{};
    static inline uint32_t width{};
    static inline uint32_t height{};

    static inline std::vector<std::function<void(int key, int scancode, int action, int mods)>> key_callbacks;
    static inline std::vector<std::function<void(double x_pos, double y_pos)>>                  cursor_callbacks;
    static inline std::vector<std::function<void(int button, int action, int mods)>>            mouse_button_callbacks;
    static inline std::vector<std::function<void(int width, int height)>>                       resize_callbacks;
};

void window_init(Window* window, uint32_t width, uint32_t height, const char* title);

void window_deinit(const Window* window);

[[nodiscard]] VkSurfaceKHR vk_surface_get(const Window* window, VkInstance instance);

void window_register_key_callback(Window* window, std::function<void(int, int, int, int)>&& fn_ptr);

void window_register_cursor_callback(Window* window, std::function<void(double, double)>&& fn_ptr);

void window_register_mouse_button_callback(Window* window, std::function<void(int, int, int)>&& fn_ptr);

void window_register_resize_callback(Window* window, std::function<void(int, int)>&& fn_ptr);
