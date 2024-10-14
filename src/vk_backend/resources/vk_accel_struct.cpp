#include "vk_accel_struct.h"

#include <vk_backend/vk_sync.h>

void accel_struct_ctx_init(AccelStructContext* accel_struct_ctx, VkDevice device, uint32_t queue_family_idx) {
    command_ctx_init(&accel_struct_ctx->cmd_ctx, device, queue_family_idx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    accel_struct_ctx->fence = vk_fence_create(device, VK_FENCE_CREATE_SIGNALED_BIT);
}

void accel_struct_ctx_add_triangles_geometry(AccelStructContext* accel_struct_ctx, VkDevice device, VmaAllocator allocator, const ExtContext* ext_ctx,
                                             VkQueue queue, std::span<const BottomLevelGeometry> bottom_level_geometries,
                                             std::span<const TopLevelInstanceRef> instance_refs) {

    for (const auto& bottom_level_geometry : bottom_level_geometries) {

        VkAccelerationStructureGeometryTrianglesDataKHR new_triangles_data{};
        new_triangles_data.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        new_triangles_data.indexData.deviceAddress  = vk_buffer_device_address_get(device, bottom_level_geometry.mesh_buffers.indices.buffer);
        new_triangles_data.vertexData.deviceAddress = vk_buffer_device_address_get(device, bottom_level_geometry.mesh_buffers.vertices.buffer);
        new_triangles_data.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT; // assumes position x,y,z is first 3 floats of each vertex
        new_triangles_data.indexType                = VK_INDEX_TYPE_UINT32;
        new_triangles_data.maxVertex                = bottom_level_geometry.vertex_count - 1;
        new_triangles_data.vertexStride             = bottom_level_geometry.vertex_stride;
        new_triangles_data.transformData            = {};

        VkAccelerationStructureGeometryKHR new_triangle_geometry{};
        new_triangle_geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        new_triangle_geometry.geometry.triangles = new_triangles_data;
        new_triangle_geometry.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        new_triangle_geometry.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;

        VkAccelerationStructureBuildRangeInfoKHR new_triangle_build_range{};
        new_triangle_build_range.primitiveCount  = bottom_level_geometry.index_count / 3;
        new_triangle_build_range.firstVertex     = 0;
        new_triangle_build_range.primitiveOffset = 0;
        new_triangle_build_range.transformOffset = 0;

        accel_struct_ctx->triangle_geometries.push_back(new_triangle_geometry);
        accel_struct_ctx->triangle_build_ranges.push_back(new_triangle_build_range);
    }

    // so now create the build infos out of what we have to ask vulkan how large things need to be
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_geometry_infos;
    std::vector<VkAccelerationStructureBuildSizesInfoKHR>    build_sizes_infos;

    build_geometry_infos.reserve(accel_struct_ctx->triangle_geometries.size());
    build_sizes_infos.reserve(accel_struct_ctx->triangle_geometries.size());

    uint32_t max_scratch_buf_size = 0;
    uint32_t as_total_size        = 0;

    for (size_t i = 0; i < accel_struct_ctx->triangle_geometries.size(); i++) {
        const VkAccelerationStructureGeometryKHR*       geometry         = &accel_struct_ctx->triangle_geometries[i];
        const VkAccelerationStructureBuildRangeInfoKHR* build_range_info = &accel_struct_ctx->triangle_build_ranges[i];

        VkAccelerationStructureBuildGeometryInfoKHR new_build_geo_info{};
        new_build_geo_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        new_build_geo_info.type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        new_build_geo_info.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        new_build_geo_info.flags =
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        new_build_geo_info.pGeometries   = geometry;
        new_build_geo_info.geometryCount = 1;

        build_geometry_infos.push_back(new_build_geo_info);

        VkAccelerationStructureBuildSizesInfoKHR new_build_sizes_info{};
        new_build_sizes_info.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        const uint32_t max_primitive_count = build_range_info->primitiveCount;
        ext_ctx->vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &new_build_geo_info,
                                                         &max_primitive_count, &new_build_sizes_info);

        build_sizes_infos.push_back(new_build_sizes_info);

        max_scratch_buf_size += new_build_sizes_info.buildScratchSize;
        as_total_size += new_build_sizes_info.accelerationStructureSize;
    }

    AllocatedBuffer scratch_buffer =
        allocated_buffer_create(allocator, max_scratch_buf_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    for (size_t i = 0; i < build_geometry_infos.size(); i++) {
        VkAccelerationStructureBuildGeometryInfoKHR*    build_geometry_info = &build_geometry_infos[i];
        const VkAccelerationStructureBuildSizesInfoKHR* build_sizes_info    = &build_sizes_infos[i];
        const VkAccelerationStructureBuildRangeInfoKHR* build_range_info    = &accel_struct_ctx->triangle_build_ranges[i];

        AllocatedBuffer blas_buffer =
            allocated_buffer_create(allocator, build_sizes_info->accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0);
        accel_struct_ctx->blas_buffers.push_back(blas_buffer);

        VkAccelerationStructureCreateInfoKHR blas_ci{};
        blas_ci.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        blas_ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blas_ci.size   = build_sizes_info->accelerationStructureSize;
        blas_ci.buffer = blas_buffer.buffer;

        VkAccelerationStructureKHR blas;

        VK_CHECK(ext_ctx->vkCreateAccelerationStructureKHR(device, &blas_ci, nullptr, &blas));

        build_geometry_info->dstAccelerationStructure  = blas;
        build_geometry_info->scratchData.deviceAddress = vk_buffer_device_address_get(device, scratch_buffer.buffer);

        command_ctx_immediate_submit(&accel_struct_ctx->cmd_ctx, device, queue, accel_struct_ctx->fence, [&](VkCommandBuffer cmd) {
            ext_ctx->vkCmdBuildAccelerationStructuresKHR(cmd, 1, build_geometry_info, &build_range_info);

            // TODO: compact the acceleration structure
        });

        accel_struct_ctx->bottom_levels.push_back(blas);
    }

    // create instances to these mesh buffers. each instance can share bottom level structures, while having different transforms;
    // TODO: it's fucked up here on down. fix it

    uint32_t reference_offset = accel_struct_ctx->triangle_geometries.size() - bottom_level_geometries.size();
    for (const auto& ref : instance_refs) {

        VkAccelerationStructureDeviceAddressInfoKHR device_address_info{};
        device_address_info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        device_address_info.accelerationStructure = accel_struct_ctx->bottom_levels[ref.mesh_idx + reference_offset];

        VkDeviceAddress accel_struct_address = ext_ctx->vkGetAccelerationStructureDeviceAddressKHR(device, &device_address_info);

        VkTransformMatrixKHR vk_transform{};
        memcpy(&vk_transform.matrix, &ref.transform, 3 * 4 * sizeof(float)); // instance expects a 3 x 4 matrix

        VkAccelerationStructureInstanceKHR new_triangle_instance{};
        // why didn't Vulkan use a union here? just a plain 64-bit uint that we have to cast two possible different pointer types to?
        new_triangle_instance.accelerationStructureReference         = accel_struct_address;
        new_triangle_instance.instanceShaderBindingTableRecordOffset = 0;
        new_triangle_instance.instanceCustomIndex                    = 0;
        new_triangle_instance.transform                              = vk_transform;
        new_triangle_instance.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        new_triangle_instance.mask                                   = 0xFF;
        accel_struct_ctx->tlas_instances.push_back(new_triangle_instance);
    }

    const uint32_t instance_buf_bytes = accel_struct_ctx->tlas_instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    accel_struct_ctx->instance_buf =
        allocated_buffer_create(allocator, instance_buf_bytes,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    VK_CHECK(vmaCopyMemoryToAllocation(allocator, accel_struct_ctx->tlas_instances.data(), accel_struct_ctx->instance_buf.allocation, 0,
                                       instance_buf_bytes));

    VkAccelerationStructureGeometryInstancesDataKHR new_instances_data{};
    new_instances_data.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    new_instances_data.data.deviceAddress = vk_buffer_device_address_get(device, accel_struct_ctx->instance_buf.buffer);
    new_instances_data.arrayOfPointers    = VK_FALSE;

    VkAccelerationStructureGeometryKHR new_instance_geometry{};
    new_instance_geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    new_instance_geometry.geometry.instances = new_instances_data;
    new_instance_geometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    new_instance_geometry.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR instance_build_geo_info{};
    instance_build_geo_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    instance_build_geo_info.type  = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    instance_build_geo_info.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    instance_build_geo_info.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
    instance_build_geo_info.geometryCount = 1;
    instance_build_geo_info.pGeometries   = &new_instance_geometry;

    VkAccelerationStructureBuildSizesInfoKHR instance_build_sizes_info{};
    instance_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    const uint32_t instance_count   = accel_struct_ctx->tlas_instances.size();
    ext_ctx->vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &instance_build_geo_info,
                                                     &instance_count, &instance_build_sizes_info);

    // make a larger scratch buffer if needed
    if (instance_build_sizes_info.buildScratchSize > scratch_buffer.info.size) {
        allocated_buffer_destroy(allocator, &scratch_buffer);
        scratch_buffer = allocated_buffer_create(allocator, instance_build_sizes_info.buildScratchSize,
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                 VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    accel_struct_ctx->tlas_buffer = allocated_buffer_create(
        allocator, instance_build_sizes_info.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_AUTO, 0);

    VkAccelerationStructureCreateInfoKHR tlas_ci{};
    tlas_ci.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlas_ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_ci.size   = instance_build_sizes_info.accelerationStructureSize;
    tlas_ci.buffer = accel_struct_ctx->tlas_buffer.buffer;

    VkAccelerationStructureKHR tlas{};
    VK_CHECK(ext_ctx->vkCreateAccelerationStructureKHR(device, &tlas_ci, nullptr, &tlas));

    instance_build_geo_info.dstAccelerationStructure  = tlas;
    instance_build_geo_info.scratchData.deviceAddress = vk_buffer_device_address_get(device, scratch_buffer.buffer);

    VkAccelerationStructureBuildRangeInfoKHR instance_build_range_info{};
    instance_build_range_info.primitiveCount  = accel_struct_ctx->tlas_instances.size();
    instance_build_range_info.primitiveOffset = 0;
    instance_build_range_info.firstVertex     = 0;
    instance_build_range_info.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* p_range_info = &instance_build_range_info;

    command_ctx_immediate_submit(&accel_struct_ctx->cmd_ctx, device, queue, accel_struct_ctx->fence, [&](VkCommandBuffer cmd) {
        ext_ctx->vkCmdBuildAccelerationStructuresKHR(cmd, 1, &instance_build_geo_info, &p_range_info);
    });

    // TODO: compact tlas

    accel_struct_ctx->top_level = tlas;

    allocated_buffer_destroy(allocator, &scratch_buffer);
}

void accel_struct_ctx_deinit(const AccelStructContext* accel_struct_ctx, const ExtContext* ext_ctx, VmaAllocator allocator, VkDevice device) {
    allocated_buffer_destroy(allocator, &accel_struct_ctx->tlas_buffer);
    allocated_buffer_destroy(allocator, &accel_struct_ctx->instance_buf);

    for (const auto& blas_buffer : accel_struct_ctx->blas_buffers) {
        allocated_buffer_destroy(allocator, &blas_buffer);
    }

    ext_ctx->vkDestroyAccelerationStructureKHR(device, accel_struct_ctx->top_level, nullptr);

    for (const auto& blas : accel_struct_ctx->bottom_levels) {
        ext_ctx->vkDestroyAccelerationStructureKHR(device, blas, nullptr);
    }

    vkDestroyFence(device, accel_struct_ctx->fence, nullptr);
    command_ctx_deinit(&accel_struct_ctx->cmd_ctx, device);
}
