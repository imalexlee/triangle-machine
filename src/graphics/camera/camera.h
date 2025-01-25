#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include "graphics/renderer/vk_frame.h"
#include <system/platform/window.h>

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

    std::vector<std::function<void()>> camera_update_callbacks;
};

void camera_init(Camera* cam, const Window* window, glm::vec3 initial_pos, glm::vec3 initial_look_at);

void camera_pan(Camera* camera, float pan_factor_x, float pan_factor_y);

void camera_zoom(Camera* camera, float zoom_factor);

void camera_orbit(Camera* camera, float pitch_angle, float yaw_angle);

WorldData camera_update(Camera* cam, uint32_t viewport_width, uint32_t viewport_height);

void camera_register_update_callback(Camera* cam, std::function<void()>&& fn_ptr);
