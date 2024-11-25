#include "camera.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <fmt/base.h>
#include <iostream>
#include <vk_backend/vk_frame.h>

static void update_view_matrix(Camera* camera);

void camera_init(Camera* cam, const Window* window, glm::vec4 initial_pos, float init_pitch_theta, float init_yaw_theta) {
    cam->position  = initial_pos;
    cam->direction = cam->position - cam->look_at;
    // cam->pitch_theta = init_pitch_theta;
    // cam->yaw_theta   = init_yaw_theta;
    float radius     = glm::length(cam->direction);
    cam->pitch_theta = glm::degrees(asin(-cam->direction.y / radius)); // Negative for Y-down
    cam->yaw_theta   = glm::degrees(atan2(cam->direction.z, cam->direction.x));
    cam->cursor_x    = window->width / 2.0;
    cam->cursor_y    = window->height / 2.0;
    update_view_matrix(cam);
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

    // TODO: only enable this type of motion in game
    /*
    double x_delta = cam->cursor_x - x_pos;
    double y_delta = cam->cursor_y - y_pos;

    cam->pitch_theta -= y_delta * 0.1;
    cam->yaw_theta += x_delta * 0.1;

    cam->cursor_x = x_pos;
    cam->cursor_y = y_pos;
*/
}

void camera_mouse_button_callback(Camera* cam, int button, int action, int mods) {
    // if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    //     if (action == GLFW_PRESS) {
    //         cam->middle_mouse_pressed = true;
    //     } else {
    //         cam->middle_mouse_pressed = false;
    //     }
    // }
    // if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    //     if (action == GLFW_PRESS) {
    //         cam->right_mouse_pressed = true;
    //     } else {
    //         cam->right_mouse_pressed = false;
    //     }
    // }
}

void update_view_matrix(Camera* camera) {
    glm::vec3 world_up = glm::vec3{0.f, -1.f, 0};
    camera->direction  = glm::normalize(camera->look_at - camera->position);
    camera->right      = glm::normalize(glm::cross(camera->direction, world_up));
    camera->up         = glm::normalize(glm::cross(camera->right, camera->direction));

    // glm::mat4 yaw_mat   = glm::toMat4(glm::angleAxis(glm::radians(camera->yaw_theta), glm::vec3{0, -1, 0}));
    // glm::mat4 pitch_mat = glm::toMat4(glm::angleAxis(glm::radians(camera->pitch_theta), glm::vec3{1, 0, 0}));
    // glm::mat4 rotation  = pitch_mat * yaw_mat;
    camera->view = glm::lookAt(camera->position, camera->look_at, world_up);
    // camera->view[1][1] *= -1.f; // Vulkans Inverted Y fix
}

void camera_pan(Camera* camera, float pan_factor_x, float pan_factor_y) {
    glm::vec3 pan_x = camera->right * -pan_factor_x;
    glm::vec3 pan_y = camera->up * -pan_factor_y;

    camera->position += pan_x + pan_y;
    camera->look_at += pan_x + pan_y;
    update_view_matrix(camera);
}

void camera_zoom(Camera* camera, float zoom_factor) {
    glm::vec3 zoom = camera->direction * zoom_factor;

    camera->position += zoom;
    camera->look_at += zoom;
    update_view_matrix(camera);
}

void camera_orbit(Camera* camera, float pitch_angle, float yaw_angle) {
    // prevent gimbal lock
    camera->pitch_theta = glm::clamp(camera->pitch_theta + pitch_angle, -89.0f, 89.0f);
    camera->yaw_theta += yaw_angle;

    float radius = glm::length(camera->position - camera->look_at);

    float pitch_rad = glm::radians(camera->pitch_theta);
    float yaw_rad   = glm::radians(camera->yaw_theta);

    glm::vec3 offset;
    offset.x = radius * cos(pitch_rad) * cos(yaw_rad);
    offset.z = radius * cos(pitch_rad) * sin(yaw_rad);
    // negate Y for Vulkan's coordinate system
    offset.y = -radius * sin(pitch_rad);

    camera->position = camera->look_at + offset;
    update_view_matrix(camera);
}

WorldData camera_update(Camera* cam, uint32_t viewport_width, uint32_t viewport_height) {

    for (const auto& update_callback : cam->camera_update_callbacks) {
        update_callback();
    }

    WorldData scene_data{};
    scene_data.view    = cam->view;
    scene_data.proj    = cam->proj;
    scene_data.cam_pos = glm::vec4(cam->position, 1.f);

    return scene_data;
}

void camera_register_update_callback(Camera* cam, std::function<void()>&& fn_ptr) { cam->camera_update_callbacks.push_back(fn_ptr); }
