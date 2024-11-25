#pragma once

#include "imgui.h"
#include "window.h"

#include <GLFW/glfw3.h>
#include <vk_backend/vk_backend.h>

struct Editor {
    ImGuiContext* imgui_ctx{};
    ImGuiIO*      imgui_io{};
    ImGuiStyle*   imgui_style{};
    ImDrawData*   imgui_draw_data{};
    int           viewport_width{16};
    int           viewport_height{9};
    int           window_width{};

    // editor state
    bool ui_resized{true};
    bool quit{false};
    bool app_data_dir_exists{false};
    bool app_data_dir_contains_files{false};

    std::filesystem::path app_data_dir{"app_data"};
    std::filesystem::path curr_scene_path{};

    // editor options
    int gizmo_mode{};
    int gizmo_op{};
};

void editor_init(Editor* editor, VkBackend* backend, struct Camera* camera, GLFWwindow* window);

void editor_update(Editor* editor, VkBackend* backend, const Window* window, struct Camera* camera, struct Scene* scene);

void editor_deinit(const Editor* editor);
