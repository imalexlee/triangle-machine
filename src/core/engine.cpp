#include "engine.h"
#include <GLFW/glfw3.h>
#include <array>
#include <core/camera.h>
#include <core/loaders/gltf_loader.h>
#include <core/scene.h>
// #include <core/ui.h>
#include <core/window.h>
#include <core/wl_window.cpp>
#include <core/wl_window.h>
#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

void init_engine(Engine* engine) {
    assert(active_engine == nullptr);
    active_engine = engine;

    // glm::vec3 init_cam_pos = {0, -1, -8};
    glm::vec3 init_cam_pos = glm::vec3(-30.f, -0, -30.f);

    // init_window(&engine->window, core_opts::initial_width, core_opts::initial_height,
    //            "Triangle Machine");

    init_wayland_client("Triangle Machine");

    init_camera(&engine->camera, wl_width, wl_height, init_cam_pos);

    VkInstance instance = create_vk_instance("triangle machine", "my engine");
    // VkSurfaceKHR surface  = get_vulkan_surface(&engine->window, instance);

    VkSurfaceKHR surface = get_wayland_surface(instance);
    init_backend(&engine->backend, instance, surface, wl_width, wl_height);
    // init_ui(&engine->ui, &engine->backend, engine->window.glfw_window);

    create_pipeline(&engine->backend, "../../shaders/vertex/indexed_triangle.vert.glsl.spv",
                    "../../shaders/fragment/simple_lighting.frag.glsl.spv");

    std::array gltf_paths = {"../../assets/glb/structure.glb"};
    load_scene(&engine->scene, &engine->backend, gltf_paths);

    // register_key_callback(&engine->window, [=](int key, int scancode, int action, int mods) {
    //     camera_key_callback(&engine->camera, key, scancode, action, mods);
    //     scene_key_callback(&engine->scene, key, action);
    // });

    // register_cursor_callback(&engine->window, [=](double x_pos, double y_pos) {
    //     camera_cursor_callback(&engine->camera, x_pos, y_pos);
    // });
}

void run_engine(Engine* engine) {

    // while (!glfwWindowShouldClose(engine->window.glfw_window)) {
    //    glfwPollEvents();

    while (!wl_quit) {

        SceneData scene_data = update_camera(&engine->camera, wl_width, wl_height);

        update_scene(&engine->scene);

        //       update_ui(&engine->backend);
        draw(&engine->backend, engine->scene.entities, &scene_data);

        wl_display_roundtrip(display);
    }

    //};
}

void deinit_engine(Engine* engine) {
    finish_pending_vk_work(&engine->backend);

    // deinit_ui(&engine->ui);
    // deinit_window(&engine->window);
    deinit_backend(&engine->backend);
    deinit_wayland_client();
};
