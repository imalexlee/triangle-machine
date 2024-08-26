#include "camera.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <fmt/base.h>
#include <glm/matrix.hpp>
#include <vk_backend/vk_frame.h>

void init_camera(Camera*   cam,
                 Window*   window,
                 glm::vec3 initial_pos,
                 float     init_pitch_theta,
                 float     init_yaw_theta) {

    cam->position    = initial_pos;
    cam->pitch_theta = init_pitch_theta;
    cam->yaw_theta   = init_yaw_theta;
    cam->cursor_x    = window->width / 2.0;
    cam->cursor_y    = window->height / 2.0;
    update_camera(cam, window->width, window->height);
}

void camera_key_callback(
    Camera* cam, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
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

void camera_cursor_callback(Camera* cam, double x_pos, double y_pos) {
    double x_delta = cam->cursor_x - x_pos;
    double y_delta = cam->cursor_y - y_pos;

    cam->pitch_theta -= y_delta * 0.1;
    cam->yaw_theta += x_delta * 0.1;

    cam->cursor_x = x_pos;
    cam->cursor_y = y_pos;
}

using namespace std::chrono;
static auto start_time = high_resolution_clock::now();

SceneData update_camera(Camera* cam, int window_width, int window_height) {
    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    glm::quat yaw_quat = glm::angleAxis(glm::radians(cam->yaw_theta), glm::vec3{0, -1, 0});
    glm::mat4 yaw_mat  = glm::toMat4(yaw_quat);

    glm::quat pitch_quat = glm::angleAxis(glm::radians(cam->pitch_theta), glm::vec3{1, 0, 0});
    glm::mat4 pitch_mat  = glm::toMat4(pitch_quat);

    glm::mat4 cam_rotation    = pitch_mat * yaw_mat;
    glm::mat4 cam_translation = glm::translate(glm::mat4{1.f}, cam->position);

    cam->position += glm::vec3(glm::vec4(cam->velocity * time_elapsed, 0.f) * cam_rotation);
    cam->view = cam_rotation * cam_translation;

    glm::mat4 projection = glm::perspective(
        glm::radians(60.f), (float)window_width / (float)window_height, 10000.0f, 0.1f);

    projection[1][1] *= -1;

    SceneData scene_data;
    scene_data.view_proj = projection * cam->view;
    scene_data.eye_pos   = cam->position;

    // auto end_time = system_clock::now();
    // auto dur      = duration<float>(end_time - start_time);
    // if (backend->frame_num % 60 == 0) {
    //     backend->stats.scene_update_time = duration_cast<nanoseconds>(dur).count() / 1000.f;
    // }

    start_time = high_resolution_clock::now();
    return scene_data;
}
