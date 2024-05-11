#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <core/window.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

constexpr float MOVEMENT_SPEED = 4.f;

class Camera {
public:
  static inline glm::vec3 position;
  static inline glm::vec3 velocity;
  static inline glm::mat4 view;

  static inline float pitch_theta;
  static inline float yaw_theta;

  static inline double cursor_x;
  static inline double cursor_y;

  void create(Window &window, glm::vec3 initial_pos, float init_pitch_theta = 0.f, float init_yaw_theta = 0.f);

  static void key_callback(int key, int scancode, int action, int mods);
  static void cursor_callback(double x_pos, double y_pos);

  static void update();
};
