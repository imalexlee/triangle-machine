#pragma once

#include "core/window.h"
#include "vk_backend/resources/vk_image.h"
#include "vk_backend/vk_command.h"
#include "vk_backend/vk_debug.h"
#include "vk_backend/vk_device.h"
#include "vk_backend/vk_frame.h"
#include "vk_backend/vk_utils.h"
#include <cstdint>
#include <filesystem>
#include <vector>
#include <vk_backend/vk_scene.h>
#include <vk_backend/vk_swapchain.h>
#include <vk_backend/vk_types.h>
#include <vulkan/vulkan_core.h>

constexpr uint64_t FRAME_NUM = 3;

class VkBackend {
public:
  VkBackend(){};
  ~VkBackend() { destroy(); };

  void create(Window& window);
  void draw();
  void resize(uint32_t width, uint32_t height);

private:
  VkInstance _instance;
  DeviceContext _device_context;
  VkSurfaceKHR _surface;
  Debugger _debugger;
  SwapchainContext _swapchain_context;
  VmaAllocator _allocator;

  GLTFScene _scene;

  CommandContext _imm_cmd_context;
  VkFence _imm_fence;

  AllocatedImage _draw_image;
  AllocatedImage _depth_image;

  uint64_t _frame_num{1};
  std::array<Frame, FRAME_NUM> _frames;
  // the per-frame data we will write to the frames' descriptor sets
  SceneData _scene_data;
  //  PipelineInfo _default_pipeline_info;

  DeletionQueue _deletion_queue;

  // initialization
  void create_instance(GLFWwindow* window);
  void create_allocator();
  void create_pipelines();
  void create_default_data();

  // core functions
  void draw_geometry(VkCommandBuffer cmd_buf, VkExtent2D extent, uint32_t swapchain_img_idx);
  void update_scene();

  // deinitialization
  void destroy();

  // utils
  inline Frame& get_current_frame() { return _frames[_frame_num % FRAME_NUM]; }
  std::vector<const char*> get_instance_extensions(GLFWwindow* window);
  void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
  MeshBuffers upload_mesh_buffers(std::span<uint32_t> indices, std::span<Vertex> vertices);

  friend GLTFScene load_scene(VkBackend* backend, std::filesystem::path path);
};
