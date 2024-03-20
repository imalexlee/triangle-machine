cmake_minimum_required(VERSION 3.22)

project(weird)
find_package(glfw3)
find_package(Vulkan REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if(UNIX AND NOT APPLE)
  add_link_options("-lglfw;-lvulkan;-lXxf86vm;-lXrandr;-lXi")
endif()

add_executable(
  ${PROJECT_NAME}
  src/application/entry.cpp src/application/application.cpp
  src/application/window.cpp src/renderer/engine.cpp
  # src/vk_engine.cpp
  # src/vk_initializers.cpp
  # src/vk_images.cpp
  # src/vk_descriptors.cpp
  # src/vk_pipelines.cpp
  # src/camera.cpp
  # src/vk_loader.cpp
  # src/vk_engine.h
  # src/vk_initializers.h
  # src/vk_images.h
  # src/vk_descriptors.h
  # src/vk_pipelines.h
  # src/camera.h
  # src/vk_loader.h)
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
if(APPLE)
  enable_language(OBJC)
endif()

add_subdirectory(thirdparty/fmt)
add_subdirectory(thirdparty/VulkanMemoryAllocator)
add_subdirectory(thirdparty/fastgltf)
add_subdirectory(thirdparty)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC src
  PUBLIC thirdparty/glm
  PUBLIC thirdparty/fmt/include
  PUBLIC thirdparty/stb_image
  PUBLIC thirdparty/fastgltf/include
  PUBLIC thirdparty/VulkanMemoryAllocator/include)
# PUBLIC $ENV{VULKAN_SDK_PATH}/include

target_link_directories(
  ${PROJECT_NAME}
  PUBLIC
  ${PROJECT_NAME}
  PUBLIC
  thirdparty/fmt/src
  PUBLIC
  thirdparty/fastgltf/src
  PUBLIC
  thirdparty/VulkanMemoryAllocator/src)
# PUBLIC $ENV{VULKAN_SDK_PATH}/lib

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE glfw
  PRIVATE fastgltf
  PRIVATE fmt
  PRIVATE VulkanMemoryAllocator
  PRIVATE Vulkan::Vulkan
  PRIVATE imgui)

find_program(
  GLSL_VALIDATOR glslangValidator
  HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK_PATH}/Bin/
        $ENV{VULKAN_SDK_PATH}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES "${PROJECT_SOURCE_DIR}/shaders/*.frag"
     "${PROJECT_SOURCE_DIR}/shaders/*.vert"
     "${PROJECT_SOURCE_DIR}/shaders/*.comp")

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)
