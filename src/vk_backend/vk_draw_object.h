#pragma once

#include "vk_backend/resources/vk_buffer.h"
#include "vk_backend/vk_utils.h"
#include <vk_backend/vk_types.h>

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

struct DrawObjectPushConstants {
  VkDeviceAddress vertex_buffer_address;
};

class DrawObject {
public:
  void create(VkDevice& device, VmaAllocator& allocator, uint32_t indices_byte_len, uint32_t vertices_byte_len);

  DrawObjectPushConstants push_constants;
  AllocatedBuffer vertex_buffer;
  AllocatedBuffer index_buffer;

private:
  DeletionQueue _deletion_queue;
};
