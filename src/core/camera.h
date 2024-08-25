#pragma once

#include <vk_backend/vk_frame.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <core/window.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

struct Camera {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::mat4 view;

    float pitch_theta;
    float yaw_theta;
    float movement_speed;

    double cursor_x;
    double cursor_y;
};

void init_camera(Camera*   cam,
                 Window*   window,
                 glm::vec3 initial_pos,
                 float     init_pitch_theta = 0.f,
                 float init_yaw_theta       = 0.f);

SceneData update_camera(Camera* cam, int window_width, int window_height);

void camera_key_callback(Camera* cam, int key, int scancode, int action, int mods);

void camera_cursor_callback(Camera* cam, double x_pos, double y_pos);
