#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera {
public:
  glm::vec3 position;
  // glm::vec3 view_direction;
  glm::mat4 look_at;

  void create(glm::vec3 initial_pos, glm::mat4 look_at);
  void update(glm::vec3 new_pos, glm::mat4 look_at);
};
