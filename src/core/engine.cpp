#include "engine.h"
#include "core_options.h"

#include <GLFW/glfw3.h>
#include <array>
#include <core/camera.h>
#include <core/editor.h>
#include <core/scene.h>
#include <core/window.h>
#include <stb_image.h>

#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

void engine_init(Engine* engine) {
    assert(active_engine == nullptr);
    active_engine = engine;

    constexpr glm::vec4 init_cam_pos = {0, -1, -8, 1};

    window_init(&engine->window, core_opts::initial_width, core_opts::initial_height, "Triangle Machine");

    camera_init(&engine->camera, &engine->window, init_cam_pos);

    const VkInstance   instance = vk_instance_create("triangle machine", "my engine");
    const VkSurfaceKHR surface  = vk_surface_get(&engine->window, instance);

    backend_init(&engine->backend, instance, surface, engine->window.width, engine->window.height);
    editor_init(&engine->editor, &engine->backend, engine->window.glfw_window);
    backend_upload_vert_shader(&engine->backend, "../shaders/vertex/indexed_draw.vert", "vert shader");
    backend_upload_frag_shader(&engine->backend, "../shaders/fragment/pbr.frag", "frag shader");

    backend_upload_sky_box_shaders(&engine->backend, "../shaders/vertex/skybox.vert", "../shaders/fragment/skybox.frag", "skybox shaders");

    std::array file_names = {
        "../assets/skybox/right.jpg",  "../assets/skybox/left.jpg",  "../assets/skybox/top.jpg",
        "../assets/skybox/bottom.jpg", "../assets/skybox/front.jpg", "../assets/skybox/back.jpg",
    };

    int width, height, nr_channels;
    stbi_info(file_names[0], &width, &height, &nr_channels);

    std::vector<uint8_t> skybox_data;
    skybox_data.resize(width * height * 4 * 6); // 4 channels. 6 total images

    for (size_t i = 0; i < 6; i++) {
        uint8_t* data      = stbi_load(file_names[i], &width, &height, &nr_channels, 4);
        size_t   data_size = width * height * 4;
        size_t   offset    = data_size * i;
        memcpy(skybox_data.data() + offset, data, data_size);
    }

    backend_upload_sky_box(&engine->backend, skybox_data.data(), 4, width, height);

    /*
    std::array gltf_paths = {// "../assets/gltf/main_sponza/pkg_a_Curtains/NewSponza_Curtains_glTF.gltf",
                             // "../assets/gltf/main_sponza/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf",
                             // "../assets/gltf/main_sponza/pkg_c_trees/NewSponza_CypressTree_glTF.gltf",
                             "../assets/gltf/main_sponza/Main1_Sponza/NewSponza_Main_glTF_003.gltf"};
                             */

    std::array gltf_paths = {
        "../assets/glb/porsche.glb",
        "../assets/glb/structure.glb",
    };
    scene_load(&engine->scene, &engine->backend, gltf_paths);

    window_register_key_callback(&engine->window, [=](int key, int scancode, int action, int mods) {
        camera_key_callback(&engine->camera, key, scancode, action, mods);
        editor_key_callback(&engine->editor, key, scancode, action, mods);
        scene_key_callback(&engine->scene, key, action);
    });

    window_register_cursor_callback(&engine->window, [=](double x_pos, double y_pos) { camera_cursor_callback(&engine->camera, x_pos, y_pos); });
}

void engine_run(Engine* engine) {
    WorldData world_data{};
    while (!glfwWindowShouldClose(engine->window.glfw_window)) {
        glfwPollEvents();

        world_data = camera_update(&engine->camera, engine->window.width, engine->window.height);

        if (engine->editor.should_recompile_shaders) {
            // TODO: don't hardcode the index
            backend_recompile_frag_shader(&engine->backend, 1);
            engine->editor.should_recompile_shaders = false;
        }

        editor_update(&engine->editor, &engine->backend);
        scene_update(&engine->scene, &engine->editor);

        backend_draw(&engine->backend, engine->scene.entities, &world_data, 0, 0);
    }
}

void engine_deinit(Engine* engine) {
    backend_finish_pending_vk_work(&engine->backend);

    editor_deinit(&engine->editor);
    window_deinit(&engine->window);
    backend_deinit(&engine->backend);
}