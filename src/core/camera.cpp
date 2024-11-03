#include "camera.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <fmt/base.h>
#include <iostream>
#include <vk_backend/vk_frame.h>

void camera_init(Camera* cam, const Window* window, glm::vec4 initial_pos, float init_pitch_theta, float init_yaw_theta) {

    cam->position    = initial_pos;
    cam->pitch_theta = init_pitch_theta;
    cam->yaw_theta   = init_yaw_theta;
    cam->direction   = {0, 0, -1.f, 0};
    cam->cursor_x    = window->width / 2.0;
    cam->cursor_y    = window->height / 2.0;
    camera_update(cam, window->width, window->height);
}

void camera_key_callback(Camera* cam, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
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

WorldData camera_update(Camera* cam, uint32_t viewport_width, uint32_t viewport_height) {
    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    glm::quat yaw_quat = glm::angleAxis(glm::radians(cam->yaw_theta), glm::vec3{0, -1, 0});
    glm::mat4 yaw_mat  = glm::toMat4(yaw_quat);

    glm::quat pitch_quat = glm::angleAxis(glm::radians(cam->pitch_theta), glm::vec3{1, 0, 0});
    glm::mat4 pitch_mat  = glm::toMat4(pitch_quat);

    glm::mat4 cam_rotation    = pitch_mat * yaw_mat;
    glm::mat4 cam_translation = glm::translate(glm::mat4{1.f}, glm::vec3(cam->position));

    cam->position += glm::vec4(cam->velocity * time_elapsed, 0.f) * cam_rotation;
    cam->view      = cam_rotation * cam_translation;
    cam->direction = cam_rotation * glm::vec4{0, 0, -1.f, 0};

    cam->proj = glm::perspective(glm::radians(55.f), static_cast<float>(viewport_width) / static_cast<float>(viewport_height), 10000.0f, 0.1f);
    cam->proj[1][1] *= -1; // correcting for Vulkans inverted Y coordinate

    WorldData scene_data{};
    scene_data.view    = cam->view;
    scene_data.proj    = cam->proj;
    scene_data.cam_pos = cam->position;

    start_time = high_resolution_clock::now();
    return scene_data;
}