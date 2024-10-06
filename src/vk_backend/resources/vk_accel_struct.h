#pragma once

#include "vk_backend/vk_ext.h"
#include "vk_buffer.h"

#include <span>
#include <vk_backend/vk_command.h>
#include <vk_backend/vk_scene.h>
#include <vulkan/vulkan_core.h>

struct TopLevelInstanceRef {
    glm::mat4 transform;
    uint32_t  mesh_idx;
};

struct BottomLevelGeometry {
    MeshBuffers mesh_buffers;
    uint32_t    vertex_stride;
    uint32_t    vertex_count;
    uint32_t    index_count;
};

// god awful
struct AccelStructContext {
    VkAccelerationStructureKHR              top_level;
    std::vector<VkAccelerationStructureKHR> bottom_levels;

    CommandContext cmd_ctx;
    VkFence        fence;

    AllocatedBuffer              instance_buf;
    AllocatedBuffer              tlas_buffer;
    std::vector<AllocatedBuffer> blas_buffers;

    std::vector<VkAccelerationStructureGeometryKHR> triangle_geometries;
    std::vector<VkAccelerationStructureInstanceKHR> tlas_instances;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> triangle_build_ranges;
};

void accel_struct_ctx_init(AccelStructContext* accel_struct_ctx, VkDevice device, uint32_t queue_family_idx);

void accel_struct_ctx_add_triangles_geometry(AccelStructContext* accel_struct_ctx, VkDevice device, VmaAllocator allocator, const ExtContext* ext_ctx,
                                             VkQueue queue, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                             std::span<const TopLevelInstanceRef> instance_refs);

void accel_struct_ctx_deinit(const AccelStructContext* accel_struct_ctx, const ExtContext* ext_ctx, VmaAllocator allocator, VkDevice device);
