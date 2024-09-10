#pragma once

#include "imgui.h"
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vk_backend/vk_backend.h>

struct UI {
    ImGuiContext* imgui_ctx;
    ImGuiIO*      imgui_io;
};

void init_ui(UI* ui, VkBackend* backend, GLFWwindow* window);

void update_ui(const VkBackend* backend);

void deinit_ui(const UI* ui);
