cmake_minimum_required(VERSION 3.29)

include(CMakePrintHelpers)

project(triangle-machine)
find_package(Vulkan REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (UNIX AND NOT APPLE)
    add_link_options("-lglfw;-lvulkan;-lXxf86vm;-lXrandr;-lXi")
endif ()

# I hate this. my game and engine are one. this is only temporary until I have more time to make a truly
# independent engine from my games
add_executable(
        ${PROJECT_NAME}
        #        src/entry.cpp
        src/entry.cpp
        src/core/engine.cpp
        src/core/window.cpp
        src/core/camera.cpp
        src/core/scene.cpp
        src/core/editor.cpp
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
        src/vk_backend/resources/vk_accel_struct.cpp
        src/vk_backend/resources/vk_accel_struct.h

        thirdparty/ImGuizmo/ImGuizmo.cpp
        thirdparty/ImGuizmo/ImGuizmo.h
        thirdparty/ImGuiFileDialog/ImGuiFileDialog.cpp
        thirdparty/ImGuiFileDialog/ImGuiFileDialog.h
        thirdparty/ImGuiFileDialog/ImGuiFileDialogConfig.h
        thirdparty/nlohmann/json.hpp
        src/core/audio.cpp
        src/core/audio.h
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

# Find NVTT.
if (NOT NVTT_DIR)
    set(_SEARCH_PATHS "$ENV{NVTT_DIR}" "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/.." "${CMAKE_CURRENT_LIST_DIR}/../nvtt" "C:/Program Files/NVIDIA Corporation/NVIDIA Texture Tools")

    foreach (_PATH ${_SEARCH_PATHS})
        file(GLOB _DLL_POSSIBILITIES "${_PATH}/nvtt*.dll" "${_PATH}/libnvtt.so.*")
        if (_DLL_POSSIBILITIES) # If this folder contains a DLL matching the NVTT DLL pattern
            set(NVTT_DIR "${_PATH}")
            break()
        endif ()
    endforeach ()

    if (NOT NVTT_DIR)
        message(WARNING "NVTT not found! Please install NVTT from https://developer.nvidia.com/nvidia-texture-tools-exporter and set the CMake NVTT_DIR variable to the folder containing nvtt*.dll (e.g. C:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools).")
        return()
    endif ()
endif ()

# Get the NVTT shared library name.
file(GLOB _NVTT_SL_POSSIBILITIES "${NVTT_DIR}/nvtt*.dll" "${NVTT_DIR}/libnvtt.so.*")
if (NOT _NVTT_SL_POSSIBILITIES)
    message(WARNING "NVTT_DIR didn't contain an NVTT shared library of the form nvtt*.dll or libnvtt.so.*! Is NVTT_DIR set correctly? NVTT_DIR was ${NVTT_DIR}")
    return()
else ()
    list(LENGTH _NVTT_SL_POSSIBILITIES _NVTT_SL_POSSIBILITIES_LEN)
    math(EXPR _NVTT_SL_IDX ${_NVTT_SL_POSSIBILITIES_LEN}-1)
    list(GET _NVTT_SL_POSSIBILITIES ${_NVTT_SL_IDX} _NVTT_SL)
endif ()

# Find the NVTT linker library on Windows.
if (WIN32)
    if (NOT NVTT_LIB)
        file(GLOB _NVTT_LIB_ALL "${NVTT_DIR}/lib/x64-v*/nvtt*.lib")
        if (NOT _NVTT_LIB_ALL)
            message(WARNING "Found nvtt.dll in ${NVTT_DIR}, but was unable to find nvtt.lib in ${NVTT_DIR}/lib/... ! Please check the NVTT directory and this CMake script to see if the path is correct.")
            return()
        endif ()
        list(LENGTH _NVTT_LIB_ALL _NVTT_LIB_LEN)
        math(EXPR _NVTT_LIB_IDX ${_NVTT_LIB_LEN}-1)
        list(GET _NVTT_LIB_ALL ${_NVTT_LIB_IDX} NVTT_LIB)
    endif ()
endif ()

# Print out information to help with error reports.
message(STATUS "NVTT Shared Library: ${_NVTT_SL}")
message(STATUS "NVTT .lib (Windows): ${NVTT_LIB}")

# Link with NVTT
if (WIN32)
    target_link_libraries("${PROJECT_NAME}" PRIVATE "${NVTT_LIB}")
else ()
    target_link_libraries("${PROJECT_NAME}" PRIVATE "${_NVTT_SL}")
endif ()

target_include_directories("${PROJECT_NAME}"
        PRIVATE
        "${NVTT_DIR}/include"
)

# Copy NVTT to the output directory.
add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND cmake -E copy_if_different "${_NVTT_SL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC glfw
        fastgltf
        fmt
        imgui
        shaderc
        shaderc_util
        Vulkan::Vulkan
        #        fmod_vc
        fmod
        ktx
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

