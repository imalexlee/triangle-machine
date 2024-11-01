#include "editor.h"

#include "window.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

void editor_init(Editor* editor, VkBackend* backend, GLFWwindow* window) {
    editor->imgui_ctx = ImGui::CreateContext();

    editor->imgui_io              = &ImGui::GetIO();
    editor->imgui_io->ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::StyleColorsDark();

    editor->imgui_style = &ImGui::GetStyle();
    // gamma corrected
    editor->imgui_style->Colors[ImGuiCol_WindowBg] = ImVec4(pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), 1.0);

    backend_create_imgui_resources(backend);
}

void editor_update(Editor* editor, const VkBackend* backend, const Window* window) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    bool show_window = true;

    ImGuiWindowFlags window_flags = 0;

    window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowSize(ImVec2{300.f, 0.0f}, ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2{0.f, 0.0f}, ImGuiCond_Once);

    ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, static_cast<float>(window->height)}, ImVec2{FLT_MAX, static_cast<float>(window->height)});

    ImGui::Begin("Stats", &show_window, window_flags);

    // ui window
    float new_window_width = ImGui::GetWindowWidth();
    if (editor->window_width != new_window_width) {
        editor->ui_resized   = true;
        editor->window_width = new_window_width;
    }

    ImGui::Text("Host buffer recording: %.3f us", backend->stats.draw_time);
    ImGui::Text("Frame time: %.3f ms (%.1f FPS)", backend->stats.frame_time, 1000.f / backend->stats.frame_time);
    ImGui::Text("Scene update time: %.3f us", backend->stats.scene_update_time);

    ImGui::InputInt("Entity Selected", &editor->selected_entity);

    ImGui::End();

    ImGui::Render();

    editor->imgui_draw_data = ImGui::GetDrawData();
}

void editor_deinit(const Editor* editor) {

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();

    ImGui::DestroyContext(editor->imgui_ctx);
}

void editor_key_callback(Editor* editor, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_R) {
            editor->should_recompile_shaders = true;
        }
    }
}
// void editor_resize_callback(Editor* editor, int width, int height) { editor->ui_resized = true; }
