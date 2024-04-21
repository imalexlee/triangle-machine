cmake_minimum_required(VERSION 3.22)

project(triangle-machine)
find_package(glfw3)
find_package(Vulkan REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(UNIX AND NOT APPLE)
  add_link_options("-lglfw;-lvulkan;-lXxf86vm;-lXrandr;-lXi")
endif()

add_executable(
  ${PROJECT_NAME}
  src/entry.cpp
  src/core/engine.cpp
  src/core/window.cpp
  src/core/camera.cpp
  src/vk_backend/vk_backend.cpp
  src/vk_backend/vk_debug.cpp
  src/vk_backend/vk_device.cpp
  src/vk_backend/vk_command.cpp
  src/vk_backend/vk_swapchain.cpp
  src/vk_backend/vk_pipeline.cpp
  src/vk_backend/vk_sync.cpp
  src/vk_backend/vk_frame.cpp
  src/vk_backend/vk_scene.cpp
  src/vk_backend/vk_vma_usage.cpp
  src/vk_backend/resources/vk_descriptor.cpp
  src/vk_backend/resources/vk_loader.cpp
  src/vk_backend/resources/vk_buffer.cpp
  src/vk_backend/resources/vk_image.cpp)

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE "$<$<CONFIG:DEBUG>:-g;-Wall;-Wextra>;-Wno-missing-field-initializers")

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_compile_options(
  ${PROJECT_NAME}
  PUBLIC
    "$<$<CONFIG:RELEASE>:-Ofast;-flto;-Wall;-Wextra>;-Wno-missing-field-initializers>"
)

if(APPLE)
  enable_language(OBJC)
endif()

add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/VulkanMemoryAllocator)
add_subdirectory(thirdparty/fastgltf)
add_subdirectory(thirdparty)

target_include_directories(
  ${PROJECT_NAME} SYSTEM
  PRIVATE src
  PRIVATE thirdparty/glm
  PRIVATE thirdparty/fmt/include
  PRIVATE thirdparty/stb_image
  PRIVATE thirdparty/fastgltf/include
  PRIVATE thirdparty/VulkanMemoryAllocator/include)

target_link_directories(
  ${PROJECT_NAME}
  PUBLIC
  thirdparty/fmt/src
  PUBLIC
  thirdparty/fastgltf/src
  PUBLIC
  thirdparty/VulkanMemoryAllocator/src)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE glfw
  PRIVATE fastgltf
  PRIVATE fmt
  PRIVATE VulkanMemoryAllocator
  PRIVATE Vulkan::Vulkan
  # PRIVATE engine PRIVATE imgui
)

find_program(
  GLSL_VALIDATOR glslangValidator
  HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK_PATH}/Bin/
        $ENV{VULKAN_SDK_PATH}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES_VERTEX
     "${PROJECT_SOURCE_DIR}/shaders/vertex/*.vert.glsl")

foreach(GLSL ${GLSL_SOURCE_FILES_VERTEX})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/vertex/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -gVS -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

file(GLOB_RECURSE GLSL_SOURCE_FILES_FRAGMENT
     "${PROJECT_SOURCE_DIR}/shaders/fragment/*.frag.glsl")

foreach(GLSL ${GLSL_SOURCE_FILES_FRAGMENT})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/fragment/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -gVS -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)
