#include "camera.h"

void Camera::create(glm::vec3 initial_pos, glm::mat4 initial_look_at) {
  position = initial_pos;
  look_at = initial_look_at;
}

void Camera::update(glm::vec3 new_pos, glm::mat4 new_look_at) {
  position = new_pos;
  look_at = new_look_at;
}
