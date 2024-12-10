#include "core/engine.h"
#include <future>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>

constexpr uint32_t ENT_ID_LIGHTER = 1 << 0;
constexpr uint32_t ENT_ID_PICTURE = 1 << 1;
constexpr uint32_t ENT_ID_FLOWERS = 1 << 2;
constexpr uint32_t ENT_ID_GAME    = 1 << 3;
constexpr uint32_t ENT_ID_TICKET  = 1 << 4;

struct GameState {
    uint16_t   curr_entity_id{};
    CursorMode cursor_mode{CursorMode::DISABLED};
    bool       footsteps_playing = false;
    int        footsteps_channel_idx{-1};
    uint32_t   selected_ent_flags{};
};

static uint16_t curr_audio_channel;

bool block_leave_porch(Engine* engine, const glm::vec3& possible_next_pos) {
    float trigger_value = -6.5;
    if (possible_next_pos.z < trigger_value) {
        //    engine->camera.velocity = {0, 0, 0};
        return true;
    }
    return false;
}

// true if should block
bool block_enter_kitchen(Engine* engine, const GameState* game_state, const glm::vec3& possible_next_pos) {
    float       trigger_value = 3.5f;
    static bool called        = false;
    if (!called && possible_next_pos.z >= trigger_value) {
        // if we already pressed picture, allow through
        if (game_state->selected_ent_flags & ENT_ID_PICTURE) {
            called = true;
            return false;
        }
        return true;
        // todo: play oops
        // audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-fearful.mp3");
    }
    return false;
}

bool block_enter_parents_room(Engine* engine, const GameState* game_state, const glm::vec3& possible_next_pos) {
    float       trigger_value = 12.1f;
    static bool called        = false;
    if (!called && possible_next_pos.z >= trigger_value) {
        // if we already pressed game and tickets, allow through
        if ((game_state->selected_ent_flags & (ENT_ID_TICKET | ENT_ID_GAME)) == (ENT_ID_TICKET | ENT_ID_GAME)) {
            called = true;
            return false;
        }
        return true;
        // todo: play oops
        // audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-fearful.mp3");
    }
    return false;
}

bool block_enter_kids_room(Engine* engine, const GameState* game_state, const glm::vec3& possible_next_pos) {
    float       trigger_value = 20.5f;
    static bool called        = false;
    if (!called && possible_next_pos.z >= trigger_value) {
        // if we already pressed flowers, allow through
        if (game_state->selected_ent_flags & (ENT_ID_FLOWERS)) {
            called = true;
            return false;
        }
        return true;
        // todo: play oops
        // audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-fearful.mp3");
    }
    return false;
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

void cam_update_callback(Engine* engine, GameState* game_state) {
    static auto start_time = std::chrono::high_resolution_clock::now();
    using namespace std::chrono;
    Camera* cam           = &engine->camera;
    auto    time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float   time_elapsed  = time_duration.count();

    glm::quat yaw_quat = glm::angleAxis(glm::radians(cam->yaw_theta), glm::vec3{0, -1, 0});
    glm::mat4 yaw_mat  = glm::toMat4(yaw_quat);

    glm::quat pitch_quat = glm::angleAxis(glm::radians(cam->pitch_theta), glm::vec3{1, 0, 0});
    glm::mat4 pitch_mat  = glm::toMat4(pitch_quat);

    glm::mat4 cam_rotation    = pitch_mat * yaw_mat;
    glm::mat4 cam_translation = glm::translate(glm::mat4{1.f}, glm::vec3(cam->position));

    glm::vec3 possible_next_pos = cam->position + glm::vec3(glm::vec4(cam->velocity * time_elapsed, 0.f));
    if (block_enter_kitchen(engine, game_state, possible_next_pos)) {
        cam->velocity = {0, 0, 0};
    } else if (block_leave_porch(engine, possible_next_pos)) {
        cam->velocity = {0, 0, 0};
    } else if (block_enter_parents_room(engine, game_state, possible_next_pos)) {
        cam->velocity = {0, 0, 0};
    } else if (block_enter_kids_room(engine, game_state, possible_next_pos)) {
        cam->velocity = {0, 0, 0};
    }

    // incorporate if you want movement towards where the player is looking
    // cam->position += glm::vec3(glm::vec4(cam->velocity * time_elapsed, 0.f) * cam_rotation);
    cam->position += glm::vec3(glm::vec4(cam->velocity * time_elapsed, 0.f));
    cam->view      = cam_rotation * cam_translation;
    cam->direction = cam_rotation * glm::vec4{0, 0, -1.f, 0};

    cam->proj =
        glm::perspective(glm::radians(45.f), static_cast<float>(engine->window.width) / static_cast<float>(engine->window.height), 10000.0f, 0.1f);
    cam->proj[1][1] *= -1; // correcting for Vulkans inverted Y coordinate

    start_time = high_resolution_clock::now();
}

void update_entity_selection_zoom(Engine* engine, GameState* game_state, uint16_t ent_id) {
    static glm::vec3 offset = {0, 0, 0};
    if (game_state->curr_entity_id == 0 && ent_id == 0) {
        return;
    }

    // hoa ent id
    if (ent_id == 6) {
        return;
    }

    if (game_state->curr_entity_id == ent_id || game_state->curr_entity_id > 0) {
        // move entity back to original position
        scene_update_entity_pos(&engine->scene, &engine->backend, game_state->curr_entity_id, -offset);

        audio_ctx_toggle_sound(&engine->audio_ctx, curr_audio_channel);
        // curr_audio_channel = ;

        game_state->curr_entity_id = 0;
    } else if (ent_id > 0) {

        // move entity to new position in front of camera
        const glm::vec3& entity_pos = engine->scene.entities[ent_id - 1].orig_pos;

        offset = (-engine->camera.position - entity_pos);
        // keep object a little bit in front of the camera
        float len = std::max(glm::length(offset) - 1.f, 0.f);
        offset    = glm::normalize(offset) * len;

        scene_update_entity_pos(&engine->scene, &engine->backend, ent_id, offset);

        game_state->curr_entity_id = ent_id;
    }
}

void play_entity_sound(Engine* engine, GameState* game_state, uint16_t ent_id) {
    std::string_view name = engine->scene.entities[ent_id - 1].name;
    if (name == "PictureClue") {
        game_state->selected_ent_flags |= ENT_ID_PICTURE;
        uint16_t channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/Clue1.mp3");
        audio_ctx_set_volume(&engine->audio_ctx, channel_idx, 1.4);
        curr_audio_channel = channel_idx;
    }
    if (name == "BoardGameClue2") {
        game_state->selected_ent_flags |= ENT_ID_GAME;
        uint16_t channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/Clue2.mp3");
        audio_ctx_set_volume(&engine->audio_ctx, channel_idx, 1.4);
        curr_audio_channel = channel_idx;
    }
    if (name == "TicketClue3") {
        game_state->selected_ent_flags |= ENT_ID_TICKET;
        uint16_t channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/tickets.mp3");
        audio_ctx_set_volume(&engine->audio_ctx, channel_idx, 1.4);
        curr_audio_channel = channel_idx;
    }
    if (name == "flowers") {
        game_state->selected_ent_flags |= ENT_ID_FLOWERS;
        uint16_t channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/parents-discussing.mp3");
        audio_ctx_set_volume(&engine->audio_ctx, channel_idx, 1.4);
        curr_audio_channel = channel_idx;
    }
    if (name == "lighter6") {
        game_state->selected_ent_flags |= ENT_ID_LIGHTER;
        uint16_t channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/memory-of-fire-recording.mp3");
        audio_ctx_set_volume(&engine->audio_ctx, channel_idx, 1.4);
        curr_audio_channel = channel_idx;
    }
}

void mouse_button_callback(Engine* engine, GameState* game_state, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // use cursor position in the center of the viewport
            uint16_t new_ent_id = engine_select_entity_at(engine, engine->window.width / 2, engine->window.height / 2);

            if (new_ent_id > 0) {
                // convert id to actual index into the array. again, 0 represents no entity
                update_entity_selection_zoom(engine, game_state, new_ent_id);
                play_entity_sound(engine, game_state, new_ent_id);
            }
        }
    }
}

void check_for_images(Engine* engine) {
    float       trigger_value = -1.5f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        called = true;
        audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/first-pictures.mp3");
    }
}

void check_for_whos_voice(Engine* engine, const GameState* game_state) {
    float       trigger_value = 3.0f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        // check that we already chose picture clue
        if (game_state->selected_ent_flags & ENT_ID_PICTURE) {
            called = true;
            audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/whos-voice.mp3");
        }
    }
}

void check_tickets_pre(Engine* engine) {
    float       trigger_value = 5.f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        called = true;
        audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/tickets_pre.mp3");
    }
}

void check_sees_game(Engine* engine) {
    float       trigger_value = 8.8f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        called = true;
        audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/games-part1.mp3");
    }
}

void check_remembers_voice(Engine* engine) {
    float       trigger_value = 11.f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        called = true;
        audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/game-asher-2.mp3");
    }
}

void check_hears_parents(Engine* engine, const GameState* game_state) {
    float       trigger_value = 16.f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        // check that flower was clicked
        if (game_state->selected_ent_flags & ENT_ID_FLOWERS) {
            called = true;
            audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-fearful.mp3");
        }
    }
}

void check_enters_room(Engine* engine) {
    float       trigger_value = 21.f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        called = true;
        audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-in-bedroom-part-1.mp3");
    }
}

void check_realization(Engine* engine, const GameState* game_state) {
    // todo: make realization happen after clicking lighter
    float       trigger_value = 27.5f;
    static bool called        = false;
    if (!called && engine->camera.position.z > trigger_value) {
        if (game_state->selected_ent_flags & ENT_ID_LIGHTER) {
            called = true;
            audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/asher-realization.mp3");
        }
    }
}

void movement_callback(Engine* engine, GameState* game_state, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {

    // move last selected entity back if player starts moving again
    if (game_state->curr_entity_id > 0) {
        update_entity_selection_zoom(engine, game_state, 0);
    }

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
        // if (key == GLFW_KEY_A) {
        //     cam->velocity.x = cam->movement_speed;
        // }
        if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) {
            cam->velocity.z = -cam->movement_speed;
        }
        // TODO: remove in final
        // if (key == GLFW_KEY_D) {
        //     cam->velocity.x = -cam->movement_speed;
        // }
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

    if (cam->velocity != glm::vec3{0, 0, 0}) {
        if (game_state->footsteps_channel_idx == -1) {
            // first time
            game_state->footsteps_channel_idx = audio_ctx_play_sound(&engine->audio_ctx, "../assets/audio/footsteps.wav", true);
            game_state->footsteps_playing     = true;
            return;
        }
        if (!game_state->footsteps_playing) {
            audio_ctx_toggle_sound(&engine->audio_ctx, game_state->footsteps_channel_idx);
            game_state->footsteps_playing = true;
        }
    } else {
        if (game_state->footsteps_playing) {
            audio_ctx_toggle_sound(&engine->audio_ctx, game_state->footsteps_channel_idx);
            game_state->footsteps_playing = false;
        }
    }

    check_for_whos_voice(engine, game_state);
    check_for_images(engine);
    check_tickets_pre(engine);
    // check_sees_game(engine);
    // check_remembers_voice(engine);
    check_hears_parents(engine, game_state);
    check_enters_room(engine);
    check_realization(engine, game_state);
    // block_leave_porch(engine);
    // block_enter_kitchen(engine, game_state);
    // std::cout << cam->position.x << " " << cam->position.y << " " << cam->position.z << std::endl;
}

void cursor_mode_change_callback(const Engine* engine, GameState* game_state, int key, [[maybe_unused]] int scancode, int action,
                                 [[maybe_unused]] int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_C) {
            if (game_state->cursor_mode == CursorMode::DISABLED) {
                game_state->cursor_mode = CursorMode::NORMAL;
            } else {
                game_state->cursor_mode = CursorMode::DISABLED;
            }
        }
    }
    window_set_cursor_mode(&engine->window, game_state->cursor_mode);
}

void rotate_selected_entity(Engine* engine, uint16_t ent_id) {
    assert(ent_id > 0);

    static auto  start_time           = std::chrono::high_resolution_clock::now();
    static float accumulated_rotation = 0.0f;

    // Calculate time elapsed since last rotation
    auto  current_time  = std::chrono::high_resolution_clock::now();
    auto  time_duration = std::chrono::duration_cast<std::chrono::duration<float>>(current_time - start_time);
    float delta_time    = time_duration.count();

    // Define a constant rotation speed (degrees per second)
    const float ROTATION_SPEED = 100.0f; // 100 degrees per second

    // Calculate rotation amount
    static float rotation_this_frame = 0.f;
    rotation_this_frame += ROTATION_SPEED * delta_time;
    if (rotation_this_frame > 360.f) {
        rotation_this_frame = 0;
    }

    // Update entity rotation
    scene_update_entity_rotation(&engine->scene, &engine->backend, ent_id, rotation_this_frame);

    start_time = current_time;
}
void play_cutscene(AudioContext* audio_ctx, const std::filesystem::path& video_path, const std::filesystem::path& audio_path) {
    cv::VideoCapture cap(video_path.string().c_str());

    if (!cap.isOpened()) {
        std::cout << "Cannot open the video file. \n";
        exit(EXIT_FAILURE);
    }

    std::string cut_scene_name = "House of Ashes";
    cv::namedWindow(cut_scene_name, cv::WINDOW_NORMAL);

    double fps         = cap.get(cv::CAP_PROP_FPS);
    double frame_delay = 1000.0 / fps;

    size_t channel_idx   = audio_ctx_play_sound(audio_ctx, audio_path.string().c_str());
    bool   close_clicked = false;
    while (cv::getWindowProperty(cut_scene_name, cv::WND_PROP_VISIBLE)) {

        cv::Mat frame;

        if (!cap.read(frame)) {
            break;
        };

        cv::imshow(cut_scene_name, frame);

        int key = cv::waitKey(frame_delay);

        // Exit if ESC key is pressed
        if (key == 27) {
            break;
        }
    }

    if (cv::getWindowProperty(cut_scene_name, cv::WND_PROP_VISIBLE)) {
        cv::destroyWindow(cut_scene_name);
    }

    cap.release();
    audio_ctx_toggle_sound(audio_ctx, channel_idx);
}

int main() {

    Engine engine{};

    engine_init(&engine, EngineMode::RELEASE);

    std::thread intro_thread(play_cutscene, &engine.audio_ctx, "../assets/video/intro.mp4", "../assets/audio/intro-audio.mp3");

    GameState game_state{};

    scene_open(&engine.scene, &engine.backend, "app_data/final.json");

    EngineFeatures features = EngineFeatures::SKY_BOX;
    engine_enable_features(&engine, features);

    const std::string gltf_path = "../assets/glb/monkey.glb";
    scene_load_gltf_path(&engine.scene, &engine.backend, gltf_path);

    engine.camera.movement_speed = 1.f;
    // engine.camera.movement_speed = 7.f;
    engine.camera.position = {5.87f, -2.68f, -5.74};

    window_register_cursor_callback(&engine.window, [&](double x_pos, double y_pos) { eye_movement_callback(&engine, x_pos, y_pos); });

    window_register_mouse_button_callback(
        &engine.window, [&](int button, int action, int mods) { mouse_button_callback(&engine, &game_state, button, action, mods); });

    window_register_key_callback(&engine.window, [&](int key, int scancode, int action, int mods) {
        movement_callback(&engine, &game_state, key, scancode, action, mods);
        cursor_mode_change_callback(&engine, &game_state, key, scancode, action, mods);
    });

    camera_register_update_callback(&engine.camera, [&]() { cam_update_callback(&engine, &game_state); });

    window_set_cursor_mode(&engine.window, CursorMode::NORMAL);

    intro_thread.join();

    audio_ctx_play_sound(&engine.audio_ctx, "../assets/audio/questMusicQuiet.wav", true);

    window_set_cursor_mode(&engine.window, CursorMode::DISABLED);

    while (engine_is_alive(&engine) && engine.camera.position.z < 30.f) {
        engine_begin_frame(&engine);

        engine_end_frame(&engine);
    }

    if (game_state.footsteps_playing) {
        audio_ctx_toggle_sound(&engine.audio_ctx, game_state.footsteps_channel_idx);
    }

    std::thread outro_thread(play_cutscene, &engine.audio_ctx, "../assets/video/outro.mp4", "../assets/audio/outro-audio.mp3");

    outro_thread.join();

    engine_deinit(&engine);
}
