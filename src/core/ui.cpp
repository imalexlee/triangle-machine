#include "ui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void init_ui(UI* ui, VkBackend* backend, GLFWwindow* window) {
    ui->imgui_ctx = ImGui::CreateContext();

    ui->imgui_io = &ImGui::GetIO();
    ui->imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ui->imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::StyleColorsDark();

    create_imgui_vk_resources(backend);
}

void update_ui(const VkBackend* backend) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    bool show_window = true;

    ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoBackground;
    window_flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("Stats", &show_window, window_flags);

    ImGui::Text("Host buffer recording: %.3f us", backend->stats.draw_time);
    ImGui::Text("Frame time: %.3f ms (%.1f FPS)", backend->stats.frame_time, 1000.f / backend->stats.frame_time);
    ImGui::Text("Scene update time: %.3f us", backend->stats.scene_update_time);

    ImGui::End();

    ImGui::Render();
}

void deinit_ui(const UI* ui) {

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();

    ImGui::DestroyContext(ui->imgui_ctx);
}
