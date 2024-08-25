#include "engine.h"
#include "core_options.h"
#include <GLFW/glfw3.h>
#include <core/camera.h>
#include <core/loaders/gltf_loader.h>
#include <core/ui.h>
#include <core/window.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

void init_engine(Engine* engine) {
    assert(active_engine == nullptr);
    active_engine = engine;

    glm::vec3 init_cam_pos = {0, -1, -8};
    // glm::vec3 init_cam_pos = {-86.7, 3.3, -30.8};

    init_window(&engine->window, core_opts::initial_width, core_opts::initial_height,
                "Triangle Machine");

    init_camera(&engine->camera, &engine->window, init_cam_pos);

    VkInstance   instance = create_vk_instance("triangle machine", "my engine");
    VkSurfaceKHR surface  = get_vulkan_surface(&engine->window, instance);

    init_backend(&engine->backend, instance, surface, engine->window.width, engine->window.height);
    init_ui(&engine->ui, &engine->backend, engine->window.glfw_window);

    register_key_callback(&engine->window, [&](int key, int scancode, int action, int mods) {
        camera_key_callback(&engine->camera, key, scancode, action, mods);
    });

    register_cursor_callback(&engine->window, [&](double x_pos, double y_pos) {
        camera_cursor_callback(&engine->camera, x_pos, y_pos);
    });
}

void run_engine(Engine* engine) {
    Entity entity = load_scene(&engine->backend, "../../assets/glb/structure.glb");
    while (!glfwWindowShouldClose(engine->window.glfw_window)) {
        glfwPollEvents();

        SceneData scene_data =
            update_camera(&engine->camera, engine->window.width, engine->window.height);

        update_ui(&engine->backend);
        draw(&engine->backend, &entity, &scene_data);
    };
}

void deinit_engine(Engine* engine) {
    finish_pending_vk_work(&engine->backend);

    deinit_ui(&engine->ui);
    deinit_window(&engine->window);
    deinit_backend(&engine->backend);
};
