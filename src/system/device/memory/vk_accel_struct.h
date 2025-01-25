#pragma once

// #include "vk_backend/vk_ext.h"
#include "vk_buffer.h"

#include <span>
#include <system/device/vk_command.h>
// #include <vk_backend/vk_scene.h>
#include <system/device/vk_ext.h>
#include <system/device/vk_sync.h>
#include <system/device/vk_types.h>
#include <vulkan/vulkan_core.h>

struct TopLevelInstanceRef {
    glm::mat4 transform{};
    uint32_t  mesh_idx{};
};

struct BottomLevelGeometry {
    MeshBuffers mesh_buffers{};
    uint32_t    vertex_stride{};
    uint32_t    vertex_count{};
    uint32_t    index_count{};
};

struct AccelStructContext {
    VkAccelerationStructureKHR              top_level = VK_NULL_HANDLE;
    std::vector<VkAccelerationStructureKHR> bottom_levels{};

    CommandContext cmd_ctx{};
    VkFence        fence{};

    AllocatedBuffer              instance_buf{};
    AllocatedBuffer              tlas_buffer{};
    AllocatedBuffer              scratch_buffer{};
    std::vector<AllocatedBuffer> blas_buffers{};

    std::vector<VkAccelerationStructureGeometryKHR> triangle_geometries{};
    // All entities have their own list of instances
    std::vector<std::vector<VkAccelerationStructureInstanceKHR>> tlas_instances{};
    std::vector<std::vector<glm::mat4>>                          local_transforms{};

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> triangle_build_ranges{};
};

void accel_struct_ctx_init(AccelStructContext* accel_struct_ctx, VkDevice device, uint32_t queue_family_idx);

void accel_struct_ctx_add_triangles_geometry(AccelStructContext* accel_struct_ctx, VkDevice device, VmaAllocator allocator, const ExtContext* ext_ctx,
                                             VkQueue queue, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                             std::span<const TopLevelInstanceRef> instance_refs);

void accel_struct_ctx_update_tlas(AccelStructContext* accel_struct_ctx, const ExtContext* ext_ctx, VkDevice device, VkQueue queue,
                                  VmaAllocator allocator, const glm::mat4* transform, uint32_t instance_idx);

void accel_struct_ctx_deinit(const AccelStructContext* accel_struct_ctx, const ExtContext* ext_ctx, VmaAllocator allocator, VkDevice device);
