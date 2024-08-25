#include "engine.h"
#include "core_options.h"
#include <GLFW/glfw3.h>
#include <core/loaders/gltf_loader.h>
#include <core/ui.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

void Engine::create() {
    assert(active_engine == nullptr);
    active_engine = this;

    glm::vec3 init_cam_pos = {0, -1, -8};
    // glm::vec3 init_cam_pos = {-86.7, 3.3, -30.8};

    _window.create(core_opts::initial_width, core_opts::initial_height, "Triangle Machine");

    _camera.create(_window, init_cam_pos);

    VkInstance   instance = create_vk_instance("triangle machine", "my engine");
    VkSurfaceKHR surface  = _window.get_vulkan_surface(instance);

    init_backend(&_backend, instance, surface, _window.width, _window.height);
    init_ui(&ui, &_backend, _window.glfw_window);

    _window.register_key_callback(_camera.key_callback);
    _window.register_cursor_callback(_camera.cursor_callback);
}

void Engine::run() {
    Entity entity = load_scene(&_backend, "../../assets/glb/structure.glb");
    while (!glfwWindowShouldClose(_window.glfw_window)) {
        glfwPollEvents();
        SceneData scene_data = _camera.update(_window.width, _window.height);
        update_ui(&_backend);
        draw(&_backend, &entity, &scene_data);
    };
}

void Engine::destroy() {
    finish_pending_vk_work(&_backend);

    deinit_ui(&ui);
    _window.destroy();
    deinit_backend(&_backend);
};
