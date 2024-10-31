#include "editor.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void editor_init(Editor* editor, VkBackend* backend, GLFWwindow* window) {
    editor->imgui_ctx = ImGui::CreateContext();

    editor->imgui_io = &ImGui::GetIO();
    editor->imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    editor->imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::StyleColorsDark();

    backend_create_imgui_resources(backend);
}

void editor_update(const VkBackend* backend) {
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
