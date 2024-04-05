#pragma once

#include "glm/ext/vector_float3.hpp"
#include <glm/vec3.hpp>

class Camera {
public:
  glm::vec3 position;
  glm::vec3 view_direction;

  void create(glm::vec3 initial_pos, glm::vec3 initial_dir);

private:
};
