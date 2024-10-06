cmake_minimum_required(VERSION 3.22)

project(triangle-machine)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (UNIX AND NOT APPLE)
    add_link_options("-lglfw;-lvulkan;-lXxf86vm;-lXrandr;-lXi")
endif ()

add_executable(
        ${PROJECT_NAME}
        src/entry.cpp
        src/core/engine.cpp
        src/core/window.cpp
        src/core/camera.cpp
        src/core/scene.cpp
        src/core/ui.cpp
        src/core/loaders/gltf_loader.cpp
        src/vk_backend/vk_backend.cpp
        src/vk_backend/vk_debug.cpp
        src/vk_backend/vk_device.cpp
        src/vk_backend/vk_command.cpp
        src/vk_backend/vk_swapchain.cpp
        src/vk_backend/vk_pipeline.cpp
        src/vk_backend/vk_sync.cpp
        src/vk_backend/vk_frame.cpp
        src/vk_backend/vk_init.cpp
        src/vk_backend/vk_vma_usage.cpp
        src/vk_backend/resources/vk_descriptor.cpp
        src/vk_backend/resources/vk_buffer.cpp
        src/vk_backend/resources/vk_image.cpp
        src/vk_backend/vk_shader.cpp
        src/vk_backend/vk_shader.h
        src/vk_backend/vk_ext.cpp
        src/vk_backend/vk_ext.h
        src/core/loaders/loader.cpp
        src/core/loaders/loader.h
        src/vk_backend/resources/vk_accel_struct.cpp
        src/vk_backend/resources/vk_accel_struct.h
        #  src/vk_backend/resources/sudo apt-get install glslang-tools_comp.cpp
)

target_compile_options(
        ${PROJECT_NAME}
        PUBLIC "$<$<CONFIG:DEBUG>:-g;>-Wall;-Wextra;-Wno-missing-field-initializers")

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_options(
        ${PROJECT_NAME}
        PUBLIC
        "$<$<CONFIG:RELEASE>:-Ofast;-fno-signed-zeros;-flto;-Wall;-Wextra>;-Wno-missing-field-initializers"
)

if (APPLE)
    enable_language(OBJC)
endif ()

add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/fastgltf)
add_subdirectory(thirdparty/meshoptimizer)
add_subdirectory(thirdparty/etc2comp)
#add_subdirectory(thirdparty/glslang)
add_subdirectory(thirdparty/shaderc)
add_subdirectory(thirdparty)

target_include_directories(
        ${PROJECT_NAME} SYSTEM
        PRIVATE src
        PRIVATE thirdparty/glm
        PRIVATE thirdparty/fmt/include
        PRIVATE thirdparty/stb_image
        PRIVATE thirdparty/VulkanMemoryAllocator
        PRIVATE thirdparty/taskflow
        PRIVATE thirdparty/etc2comp
        PRIVATE thirdparty/etc2comp/EtcTool
        PRIVATE thirdparty/etc2comp/EtcLib/Etc
        PRIVATE thirdparty/etc2comp/EtcLib/EtcCodec
        PRIVATE thirdparty/fastgltf/include
        PRIVATE thirdparty/shaderc/utils
)

target_link_directories(${PROJECT_NAME} PUBLIC thirdparty/fmt/src PUBLIC
        thirdparty/fastgltf/src)

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC glfw
        meshoptimizer
        fastgltf
        fmt
        imgui
        EtcLib
        shaderc
        shaderc_util
        assimp
        Vulkan::Vulkan)

target_sources(
        ${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/../thirdparty/etc2comp/EtcTool/EtcFile.cpp)
target_sources(
        ${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/../thirdparty/etc2comp/EtcTool/EtcFileHeader.cpp
)

find_program(
        GLSL_VALIDATOR glslangValidator
        HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK_PATH}/Bin/
        $ENV{VULKAN_SDK_PATH}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES_VERTEX
        "${PROJECT_SOURCE_DIR}/shaders/vertex/*.vert")

foreach (GLSL ${GLSL_SOURCE_FILES_VERTEX})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/vertex/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} --target-env vulkan1.3 -gVS -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

file(GLOB_RECURSE GLSL_SOURCE_FILES_FRAGMENT
        "${PROJECT_SOURCE_DIR}/shaders/fragment/*.frag")

foreach (GLSL ${GLSL_SOURCE_FILES_FRAGMENT})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/fragment/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} --target-env vulkan1.3 -gVS -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach (GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)

