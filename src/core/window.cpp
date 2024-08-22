#include <global_utils.h>
#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "window.h"
#include <cstdlib>
#include <fmt/base.h>
#include <vk_backend/vk_debug.h>

void Window::destroy() {
    DEBUG_PRINT("destroying GLFW window");
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
};

void Window::create(uint32_t width, uint32_t height, const char* title) {

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    this->width = width;
    this->height = height;
    _title = title;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfw_window = glfwCreateWindow(width, height, _title, nullptr, nullptr);

    if (!glfw_window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // infinite cursor movement. no visible cursor
    //  glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // try to enable unscaled and unaccelerated cursor capture
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(glfw_window, key_callback);
    glfwSetCursorPosCallback(glfw_window, cursor_callback);
    glfwSetWindowSizeCallback(glfw_window, resize_callback);
}

VkSurfaceKHR Window::get_vulkan_surface(const VkInstance instance) {
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, glfw_window, nullptr, &surface));
    return surface;
}

void Window::register_key_callback(std::function<void(int, int, int, int)> fn_ptr) {
    _key_callbacks.push_back(fn_ptr);
}

void Window::register_resize_callback(std::function<void(int, int)> fn_ptr) {
    _resize_callbacks.push_back(fn_ptr);
}

void Window::register_cursor_callback(std::function<void(double, double)> fn_ptr) {
    _cursor_callbacks.push_back(fn_ptr);
}

void Window::key_callback(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action,
                          [[maybe_unused]] int mods) {

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    for (auto& callback : _key_callbacks) {
        callback(key, scancode, action, mods);
    }
}

void Window::cursor_callback([[maybe_unused]] GLFWwindow* window, double x_pos, double y_pos) {
    for (auto& callback : _cursor_callbacks) {
        callback(x_pos, y_pos);
    }
};

void Window::resize_callback([[maybe_unused]] GLFWwindow* window, int new_width, int new_height) {
    for (auto& callback : _resize_callbacks) {
        callback(new_width, new_height);
    }
}

void Window::error_callback([[maybe_unused]] int error, const char* description) {
    fmt::println("GLFW errow: {}", description);
}
