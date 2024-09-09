#include "engine.h"
#include "core_options.h"
#include <GLFW/glfw3.h>
#include <array>
#include <core/camera.h>
#include <core/scene.h>
#include <core/ui.h>
#include <core/window.h>
#include <glm/vec3.hpp>
#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

void init_engine(Engine* engine) {
    assert(active_engine == nullptr);
    active_engine = engine;

    constexpr glm::vec3 init_cam_pos = {0, -1, -8};

    init_window(&engine->window, core_opts::initial_width, core_opts::initial_height,
                "Triangle Machine");

    init_camera(&engine->camera, &engine->window, init_cam_pos);

    const VkInstance   instance = create_vk_instance("triangle machine", "my engine");
    const VkSurfaceKHR surface  = get_vulkan_surface(&engine->window, instance);

    init_backend(&engine->backend, instance, surface, engine->window.width, engine->window.height);
    init_ui(&engine->ui, &engine->backend, engine->window.glfw_window);

    create_pipeline(&engine->backend, "../shaders/vertex/indexed_draw.vert.glsl.spv",
                    "../shaders/fragment/simple_lighting_2.frag.glsl.spv");

    std::array gltf_paths = {
        "../assets/gltf/main_sponza/pkg_a_Curtains/NewSponza_Curtains_glTF.gltf",
        "../assets/gltf/main_sponza/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf",
        "../assets/gltf/main_sponza/pkg_c_trees/NewSponza_CypressTree_glTF.gltf",
        "../assets/gltf/main_sponza/Main.1_Sponza/NewSponza_Main_glTF_002.gltf"};
    load_scene(&engine->scene, &engine->backend, gltf_paths);

    register_key_callback(&engine->window, [=](int key, int scancode, int action, int mods) {
        camera_key_callback(&engine->camera, key, scancode, action, mods);
        scene_key_callback(&engine->scene, key, action);
    });

    register_cursor_callback(&engine->window, [=](double x_pos, double y_pos) {
        camera_cursor_callback(&engine->camera, x_pos, y_pos);
    });
}

void run_engine(Engine* engine) {

    while (!glfwWindowShouldClose(engine->window.glfw_window)) {
        glfwPollEvents();

        SceneData scene_data =
            update_camera(&engine->camera, engine->window.width, engine->window.height);
        update_scene(&engine->scene);

        update_ui(&engine->backend);
        draw(&engine->backend, engine->scene.entities, &scene_data);
    };
}

void deinit_engine(Engine* engine) {
    finish_pending_vk_work(&engine->backend);

    deinit_ui(&engine->ui);
    deinit_window(&engine->window);
    deinit_backend(&engine->backend);
};
