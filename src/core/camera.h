#pragma once

#include <vk_backend/vk_frame.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <core/window.h>
// #include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

struct Camera {
    glm::vec4 position{};
    glm::vec4 direction{};
    glm::vec3 velocity{};
    glm::mat4 view{};

    float pitch_theta{};
    float yaw_theta{};
    float movement_speed = 30.f;

    double cursor_x{};
    double cursor_y{};
};

void camera_init(Camera* cam, const Window* window, glm::vec4 initial_pos, float init_pitch_theta = 0.f, float init_yaw_theta = 0.f);

SceneData camera_update(Camera* cam, uint32_t window_width, uint32_t window_height);

void camera_key_callback(Camera* cam, int key, int scancode, int action, int mods);

void camera_cursor_callback(Camera* cam, double x_pos, double y_pos);
