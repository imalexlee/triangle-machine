cmake_minimum_required(VERSION 3.29)

include(CMakePrintHelpers)

project(triangle-machine)

find_package(Vulkan REQUIRED)

# thank you https://github.com/opencv/opencv/issues/6132
set(OpenCV_DIR $ENV{OpenCV_DIR}/x64/vc16/lib)
set(OpenCV_STATIC ON)

find_package(OpenCV REQUIRED)

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
        src/system/platform/window.cpp
        src/graphics/camera/camera.cpp
        src/scene/scene.cpp
        src/editor/editor.cpp
        src/resources/loaders/gltf_loader.cpp
        src/graphics/renderer/vk_renderer.cpp
        src/system/device/vk_debug.cpp
        src/system/device/vk_device.cpp
        src/system/device/vk_command.cpp
        src/graphics/renderer/vk_swapchain.cpp
        src/graphics/renderer/vk_pipeline.cpp
        src/system/device/vk_sync.cpp
        src/graphics/renderer/vk_frame.cpp
        src/graphics/renderer/vk_init.cpp
        src/system/device/memory/vk_descriptor.cpp
        src/system/device/memory/vk_buffer.cpp
        src/system/device/memory/vk_image.cpp
        src/graphics/renderer/vk_shader.cpp
        src/graphics/renderer/vk_shader.h
        src/system/device/vk_ext.cpp
        src/system/device/vk_ext.h
        src/system/device/memory/vk_accel_struct.cpp
        src/system/device/memory/vk_accel_struct.h

        thirdparty/ImGuizmo/ImGuizmo.cpp
        thirdparty/ImGuizmo/ImGuizmo.h
        thirdparty/ImGuiFileDialog/ImGuiFileDialog.cpp
        thirdparty/ImGuiFileDialog/ImGuiFileDialog.h
        thirdparty/ImGuiFileDialog/ImGuiFileDialogConfig.h
        thirdparty/nlohmann/json.hpp
        src/audio/audio.cpp
        src/audio/audio.h
        src/editor/editor.h
        src/scene/scene.h
        src/system/device/vk_context.cpp
        src/system/device/vk_context.h
        src/system/device/vk_device.h
        src/system/device/vk_debug.h
        src/system/device/vk_options.h
        src/system/device/vk_types.h
        src/system/device/vk_vma_usage.cpp
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
add_subdirectory(thirdparty/ImGuiFileDialog)
#add_subdirectory(thirdparty/meshoptimizer)
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/KTX-Software)
#add_subdirectory(thirdparty/glslang)
add_subdirectory(thirdparty/shaderc)
add_subdirectory(thirdparty)

set(FMOD_PATH "C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/core")

target_include_directories(
        ${PROJECT_NAME} SYSTEM
        PRIVATE src
        PRIVATE thirdparty/glm
        PRIVATE thirdparty/ImGuizmo
        PRIVATE thirdparty/glfw/include
        PRIVATE thirdparty/fmt/include
        PRIVATE thirdparty/stb_image
        PRIVATE thirdparty/VulkanMemoryAllocator
        PRIVATE thirdparty/taskflow
        PRIVATE thirdparty/fastgltf/include
        PRIVATE thirdparty/shaderc/utils
        PRIVATE thirdparty/ImGuiFileDialog
        PRIVATE thirdparty/
        PRIVATE ${FMOD_PATH}/inc/
        PRIVATE ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PUBLIC thirdparty/fmt/src PUBLIC
        thirdparty/fastgltf/src PUBLIC ${FMOD_PATH}/lib/x64)

add_library(fmod SHARED IMPORTED)
set_target_properties(fmod PROPERTIES
        IMPORTED_LOCATION "${FMOD_PATH}/lib/x64/fmod.dll"
        IMPORTED_IMPLIB "${FMOD_PATH}/lib/x64/fmod_vc.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${FMOD_PATH}/inc/"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FMOD_PATH}/lib/x64/fmod.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)


target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC glfw
        fastgltf
        fmt
        imgui
        shaderc
        shaderc_util
        Vulkan::Vulkan
        fmod
        ktx
        ${OpenCV_LIBS}
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

