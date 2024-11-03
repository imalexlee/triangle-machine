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

    glm::vec4 position{};
    glm::vec4 direction{};
    glm::vec3 velocity{};

    double cursor_x{};
    double cursor_y{};

    float pitch_theta{};
    float yaw_theta{};
    float movement_speed = 5.f;
};

void camera_init(Camera* cam, const Window* window, glm::vec4 initial_pos, float init_pitch_theta = 0.f, float init_yaw_theta = 0.f);

WorldData camera_update(Camera* cam, uint32_t viewport_width, uint32_t viewport_height);

void camera_key_callback(Camera* cam, int key, int scancode, int action, int mods);

void camera_cursor_callback(Camera* cam, double x_pos, double y_pos);
