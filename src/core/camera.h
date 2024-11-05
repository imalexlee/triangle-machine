#pragma once

#include <vk_backend/vk_frame.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <core/window.h>
// #include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

struct Camera {
    glm::mat4 view{};
    glm::mat4 proj{};

    glm::vec3 position{};
    glm::vec3 direction{};
    glm::vec3 look_at{0};
    glm::vec3 up{0, -1, 0};
    glm::vec3 right{1, 0, 0};
    glm::vec3 velocity{};

    double cursor_x{};
    double cursor_y{};

    float pitch_theta{};
    float yaw_theta{};
    float movement_speed = 5.f;

    bool middle_mouse_pressed{false};
    bool right_mouse_pressed{false};
};

void camera_init(Camera* cam, const Window* window, glm::vec4 initial_pos, float init_pitch_theta = 0.f, float init_yaw_theta = 0.f);

void camera_pan(Camera* camera, float pan_factor_x, float pan_factor_y);

void camera_zoom(Camera* camera, float zoom_factor);

void camera_orbit(Camera* camera, float pitch_angle, float yaw_angle);

WorldData camera_update(Camera* cam, uint32_t viewport_width, uint32_t viewport_height);

void camera_key_callback(Camera* cam, int key, int scancode, int action, int mods);

void camera_cursor_callback(Camera* cam, double x_pos, double y_pos);

void camera_mouse_button_callback(Camera* cam, int button, int action, int mods);
