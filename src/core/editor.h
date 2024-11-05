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
    int           selected_entity{-1};
    int           window_width{};
    // Notifies consumer of user requesting recompilation but doesn't mandate it.
    // Therefore, this flag should be turned off by the consumer once/if it's handled.
    bool should_recompile_shaders;
    bool ui_resized{true};

    // editor options
    int gizmo_mode{};
    int gizmo_op{};
};

void editor_init(Editor* editor, VkBackend* backend, GLFWwindow* window);

void editor_update(Editor* editor, const VkBackend* backend, const Window* window, struct Camera* camera, struct Scene* scene);

void editor_deinit(const Editor* editor);

void editor_key_callback(Editor* editor, int key, int scancode, int action, int mods);

// void editor_resize_callback(Editor* editor, int width, int height);
