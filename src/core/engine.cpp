#include "engine.h"
#include "core_options.h"
#include "resources/loaders/gltf_loader.h"
#include "system/device/vk_context.h"

#include <GLFW/glfw3.h>
#include <array>
#include <editor/editor.h>
#include <graphics/camera/camera.h>
#include <scene/scene.h>
#include <stb_image.h>
#include <system/platform/window.h>

static Engine* active_engine = nullptr;
void           movement_callback(Engine* engine, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {

    if (key != GLFW_KEY_W && key != GLFW_KEY_A && key != GLFW_KEY_S && key != GLFW_KEY_D && key != GLFW_KEY_UP && key != GLFW_KEY_DOWN) {
        return;
    }

    Camera* cam = &engine->camera;
    if (action == GLFW_PRESS) {
        // printf("hi\n");
        if (key == GLFW_KEY_W || key == GLFW_KEY_UP) {
            cam->velocity.z = cam->movement_speed;
        }
        // TODO: remove in final
        if (key == GLFW_KEY_A) {
            cam->velocity.x = cam->movement_speed;
        }
        if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) {
            cam->velocity.z = -cam->movement_speed;
        }
        // TODO: remove in final
        if (key == GLFW_KEY_D) {
            cam->velocity.x = -cam->movement_speed;
        }
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_W || key == GLFW_KEY_UP) {
            cam->velocity.z = 0;
        }
        if (key == GLFW_KEY_A) {
            cam->velocity.x = 0;
        }
        if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) {
            cam->velocity.z = 0;
        }
        if (key == GLFW_KEY_D) {
            cam->velocity.x = 0;
        }
    }
}
void eye_movement_callback(Engine* engine, double x_pos, double y_pos) {
    Camera* cam     = &engine->camera;
    double  x_delta = cam->cursor_x - x_pos;
    double  y_delta = cam->cursor_y - y_pos;

    cam->pitch_theta -= y_delta * 0.1;
    cam->yaw_theta += x_delta * 0.1;

    cam->cursor_x = x_pos;
    cam->cursor_y = y_pos;
}

void engine_init(Engine* engine, EngineMode mode) {
    assert(active_engine == nullptr);
    active_engine = engine;

    engine->mode = mode;

    constexpr glm::vec4 init_cam_pos = {4.85, 1.58, -7.95, 1};

    // constexpr glm::vec4 init_cam_pos = {0, 0, 0, 1};

    window_init(&engine->window, core_opts::initial_width, core_opts::initial_height, "Triangle Machine");

    camera_init(&engine->camera, &engine->window, init_cam_pos, glm::vec3{0, 0, 0});

    audio_ctx_init(&engine->audio_ctx);

    vk_context_init(&engine->vk_ctx, &engine->window);

    renderer_init(&engine->renderer, &engine->vk_ctx, engine->window.width, engine->window.height, mode);
    if (mode == EngineMode::EDIT) {
        editor_init(&engine->editor, &engine->renderer, &engine->camera, engine->window.glfw_window);
    }

    renderer_upload_vert_shader(&engine->renderer, "../shaders/vertex/indexed_draw.vert", "vert shader");
    renderer_upload_frag_shader(&engine->renderer, "../shaders/fragment/pbr_gltf.frag", "frag shader");
    renderer_upload_sky_box_shaders(&engine->renderer, "../shaders/vertex/skybox.vert", "../shaders/fragment/skybox.frag", "skybox shaders");

    // const char* smote_path = "../assets/skybox/smote/smote.jpeg";
    //  std::array  file_names = {
    //      "../assets/skybox/right.jpg",  "../assets/skybox/left.jpg",  "../assets/skybox/top.jpg",
    //      "../assets/skybox/bottom.jpg", "../assets/skybox/front.jpg", "../assets/skybox/back.jpg",
    //  };
    //    std::array file_names = {smote_path, smote_path, smote_path, smote_path, smote_path, smote_path};

    std::array file_names = {
        "../assets/skybox/night/px.png", "../assets/skybox/night/nx.png", "../assets/skybox/night/py.png",
        "../assets/skybox/night/ny.png", "../assets/skybox/night/pz.png", "../assets/skybox/night/nz.png",
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

    renderer_upload_sky_box(&engine->renderer, skybox_data.data(), 4, width, height);

    renderer_upload_cursor_shaders(&engine->renderer);

    window_register_cursor_callback(&engine->window, [=](double x_pos, double y_pos) {
        //        eye_movement_callback(engine, x_pos, y_pos);
        //        camera_cursor_callback(&engine->camera, x_pos, y_pos);
    });

    window_register_mouse_button_callback(&engine->window, [=](int button, int action, int mods) {
        // camera_mouse_button_callback(&engine->camera, button, action, mods);
    });

    // window_register_key_callback(&engine->window,
    //                              [&](int key, int scancode, int action, int mods) { movement_callback(engine, key, scancode, action, mods); });
}

bool engine_is_alive(const Engine* engine) { return !glfwWindowShouldClose(engine->window.glfw_window); }

void engine_begin_frame(const Engine* engine) { glfwPollEvents(); }

void engine_end_frame(Engine* engine) {

    WorldData world_data{};
    uint32_t  viewport_width  = engine->window.width;
    uint32_t  viewport_height = engine->window.height;

    if (engine->mode == EngineMode::EDIT) {
        viewport_width  = engine->editor.viewport_width;
        viewport_height = engine->editor.viewport_height;
    }
    world_data = camera_update(&engine->camera, viewport_width, viewport_height);

    if (engine->mode == EngineMode::EDIT) {
        editor_update(&engine->editor, &engine->renderer, &engine->window, &engine->camera, &engine->scene);

        if (engine->editor.quit) {
            glfwSetWindowShouldClose(engine->window.glfw_window, GLFW_TRUE);
        }
    }
    scene_request_update(&engine->scene);
    scene_update(&engine->scene, &engine->renderer);
    renderer_draw(&engine->renderer, engine->scene.entities, &world_data, engine->features);
}

void engine_deinit(Engine* engine) {
    renderer_finish_pending_vk_work(&engine->renderer);

    if (engine->mode == EngineMode::EDIT) {
        editor_deinit(&engine->editor);
    }
    window_deinit(&engine->window);
    renderer_deinit(&engine->renderer);

    vk_context_deinit(&engine->vk_ctx);
}

[[nodiscard]] uint16_t engine_select_entity_at(const Engine* engine, int32_t x, int32_t y) {
    return renderer_entity_id_at_pos(&engine->renderer, x, y);
}

void engine_enable_features(Engine* engine, EngineFeatures features) { engine->features |= features; }

void engine_disable_features(Engine* engine, EngineFeatures features) { engine->features &= ~features; }
