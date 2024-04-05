#include "camera.h"

void Camera::create(glm::vec3 initial_pos, glm::vec3 initial_dir) {
  position = initial_pos;
  view_direction = initial_dir;
}
