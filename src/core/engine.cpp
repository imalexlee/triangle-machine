#include "engine.h"
#include "core_options.h"
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

static Engine* active_engine = nullptr;

void Engine::create() {
    assert(active_engine == nullptr);
    active_engine = this;

    glm::vec3 init_cam_pos = {0, -1, -8};
    // glm::vec3 init_cam_pos = {-86.7, 3.3, -30.8};

    _window.create(core_opts::initial_width, core_opts::initial_height, "Triangle Machine");

    _camera.create(_window, init_cam_pos);

    _backend.create(_window, _camera);

    _window.register_key_callback(_camera.key_callback);
    _window.register_cursor_callback(_camera.cursor_callback);
}

void Engine::run() {
    while (!glfwWindowShouldClose(_window.glfw_window)) {
        glfwPollEvents();
        _backend.draw();
    };
}

void Engine::destroy() {
    _backend.destroy();
    _window.destroy();
};
