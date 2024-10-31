#pragma once

#include "imgui.h"
#include <GLFW/glfw3.h>
#include <vk_backend/vk_backend.h>

struct Editor {
    ImGuiContext* imgui_ctx{};
    ImGuiIO*      imgui_io{};
    // Notifies consumer of user requesting recompilation but doesn't mandate it.
    // Therefore, this flag should be turned off by the consumer once/if it's handled.
    bool should_recompile_shaders;
};

void editor_init(Editor* editor, VkBackend* backend, GLFWwindow* window);

void editor_update(const VkBackend* backend);

void editor_deinit(const Editor* editor);

void editor_key_callback(Editor* editor, int key, int scancode, int action, int mods);
