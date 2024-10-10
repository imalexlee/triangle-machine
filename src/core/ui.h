#pragma once

#include "imgui.h"
#include <GLFW/glfw3.h>
#include <vk_backend/vk_backend.h>

struct UI {
    ImGuiContext* imgui_ctx{};
    ImGuiIO*      imgui_io{};
};

void ui_init(UI* ui, VkBackend* backend, GLFWwindow* window);

void ui_update(const VkBackend* backend);

void ui_deinit(const UI* ui);
