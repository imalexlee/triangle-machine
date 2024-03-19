cmake_minimum_required(VERSION 3.20)

find_package(Vulkan REQUIRED)

set(IMGUI_PATH imgui)
file(
  GLOB
  IMGUI_GLOB
  ${IMGUI_PATH}/imgui.h
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imconfig.h
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_internal.h
  ${IMGUI_PATH}/imstb_rectpack.h
  ${IMGUI_PATH}/imstb_textedit.h
  ${IMGUI_PATH}/imstb_truetype.h
  ${IMGUI_PATH}/imgui_tables.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  # specific bindings...
  ${IMGUI_PATH}/backends/imgui_impl_glfw.h
  ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
  ${IMGUI_PATH}/backends/imgui_impl_vulkan.h
  ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp)

add_library("imgui" STATIC ${IMGUI_GLOB})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH} ${IMGUI_PATH}/backends)
target_link_libraries("imgui" PRIVATE glfw Vulkan::Vulkan)
