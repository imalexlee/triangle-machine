#include "core/engine.h"

struct GameState {
    uint16_t curr_entity_id{};
};
void eye_movement_callback(Engine* engine, double x_pos, double y_pos) {
    Camera* cam     = &engine->camera;
    double  x_delta = cam->cursor_x - x_pos;
    double  y_delta = cam->cursor_y - y_pos;

    cam->pitch_theta -= y_delta * 0.1;
    cam->yaw_theta += x_delta * 0.1;

    cam->cursor_x = x_pos;
    cam->cursor_y = y_pos;
}

void cam_update_callback(Engine* engine) {
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

    cam->position += glm::vec3(glm::vec4(cam->velocity * time_elapsed, 0.f) * cam_rotation);
    cam->view      = cam_rotation * cam_translation;
    cam->direction = cam_rotation * glm::vec4{0, 0, -1.f, 0};

    cam->proj =
        glm::perspective(glm::radians(45.f), static_cast<float>(engine->window.width) / static_cast<float>(engine->window.height), 10000.0f, 0.1f);
    cam->proj[1][1] *= -1; // correcting for Vulkans inverted Y coordinate

    start_time = high_resolution_clock::now();
}

void update_entity_selection_zoom(Engine* engine, GameState* game_state, uint16_t ent_id) {
    if (game_state->curr_entity_id == 0 && ent_id == 0) {
        return;
    }

    if (game_state->curr_entity_id == ent_id || game_state->curr_entity_id > 0) {
        // deselecting - move entity back to original position
        scene_update_entity_pos(&engine->scene, game_state->curr_entity_id, engine->scene.entities[game_state->curr_entity_id - 1].orig_pos);

        game_state->curr_entity_id = 0;
    } else if (ent_id > 0) {
        // Move entity to new position in front of camera
        glm::vec3 new_pos = -engine->camera.position + glm::normalize(engine->camera.position - engine->scene.entities[ent_id - 1].orig_pos) * 5.f;

        scene_update_entity_pos(&engine->scene, ent_id, new_pos);
        game_state->curr_entity_id = ent_id;
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
            }
        }
    }
}

void movement_callback(Engine* engine, GameState* game_state, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {

    // move last selected entity back if player starts moving again
    if (game_state->curr_entity_id > 0) {
        update_entity_selection_zoom(engine, game_state, 0);
    }

    Camera* cam = &engine->camera;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_W) {
            cam->velocity.z = cam->movement_speed;
        }
        if (key == GLFW_KEY_A) {
            cam->velocity.x = cam->movement_speed;
        }
        if (key == GLFW_KEY_S) {
            cam->velocity.z = -cam->movement_speed;
        }
        if (key == GLFW_KEY_D) {
            cam->velocity.x = -cam->movement_speed;
        }
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_W) {
            cam->velocity.z = 0;
        }
        if (key == GLFW_KEY_A) {
            cam->velocity.x = 0;
        }
        if (key == GLFW_KEY_S) {
            cam->velocity.z = 0;
        }
        if (key == GLFW_KEY_D) {
            cam->velocity.x = 0;
        }
    }
}

int main() {
    Engine engine{};

    engine_init(&engine, EngineMode::RELEASE);

    GameState game_state{};

    // scene_open(&engine.scene, &engine.backend, "app_data/game.json");

    const std::string gltf_path = "../assets/glb/porsche.glb";
    scene_load_gltf_path(&engine.scene, &engine.backend, gltf_path);

    window_register_cursor_callback(&engine.window, [&](double x_pos, double y_pos) { eye_movement_callback(&engine, x_pos, y_pos); });

    window_register_mouse_button_callback(
        &engine.window, [&](int button, int action, int mods) { mouse_button_callback(&engine, &game_state, button, action, mods); });

    window_register_key_callback(
        &engine.window, [&](int key, int scancode, int action, int mods) { movement_callback(&engine, &game_state, key, scancode, action, mods); });

    camera_register_update_callback(&engine.camera, [&]() { cam_update_callback(&engine); });

    window_set_cursor_mode(&engine.window, CursorMode::DISABLED);

    engine_run(&engine);

    engine_deinit(&engine);
}
