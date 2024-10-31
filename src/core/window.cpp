#include <functional>
#include <global_utils.h>
#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include "window.h"
#include <cstdlib>
#include <fmt/base.h>
#include <vk_backend/vk_debug.h>

static void cursor_callback(GLFWwindow* window, double x_pos, double y_pos);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void error_callback(int error, const char* description);

void window_init(Window* window, uint32_t width, uint32_t height, const char* title) {

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    window->width  = width;
    window->height = height;
    window->title  = title;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window->glfw_window = glfwCreateWindow(width, height, window->title, nullptr, nullptr);

    if (!window->glfw_window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // infinite cursor movement. no visible cursor
    //    glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //    try to enable unscaled and unaccelerated cursor capture
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window->glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    glfwSetErrorCallback(error_callback);
    glfwSetKeyCallback(window->glfw_window, key_callback);
    glfwSetCursorPosCallback(window->glfw_window, cursor_callback);
}

void window_deinit(const Window* window) {
    DEBUG_PRINT("destroying GLFW window");
    glfwDestroyWindow(window->glfw_window);
    glfwTerminate();
}

VkSurfaceKHR vk_surface_get(const Window* window, VkInstance instance) {
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window->glfw_window, nullptr, &surface));
    return surface;
}

void window_register_key_callback(Window* window, std::function<void(int, int, int, int)>&& fn_ptr) { window->key_callbacks.push_back(fn_ptr); }

void window_register_cursor_callback(Window* window, std::function<void(double, double)>&& fn_ptr) { window->cursor_callbacks.push_back(fn_ptr); }

void key_callback(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    for (auto& callback : Window::key_callbacks) {
        callback(key, scancode, action, mods);
    }
}

void cursor_callback([[maybe_unused]] GLFWwindow* window, double x_pos, double y_pos) {
    for (auto& callback : Window::cursor_callbacks) {
        callback(x_pos, y_pos);
    }
}

void error_callback([[maybe_unused]] int error, const char* description) { fmt::println("GLFW errow: {}", description); }
