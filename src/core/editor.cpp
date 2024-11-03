#include "editor.h"

#include "camera.h"
#include "scene.h"
#include "window.h"

#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

void editor_init(Editor* editor, VkBackend* backend, GLFWwindow* window) {
    editor->imgui_ctx = ImGui::CreateContext();

    editor->imgui_io              = &ImGui::GetIO();
    editor->imgui_io->ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::StyleColorsDark();

    editor->imgui_style = &ImGui::GetStyle();
    // gamma corrected
    editor->imgui_style->Colors[ImGuiCol_WindowBg] = ImVec4(pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), 1.0);
    // editor->imgui_style->Colors[ImGuiCol_WindowBg].w = 1.0f;

    backend_create_imgui_resources(backend);
}

void editor_update_viewport(Editor* editor, const VkBackend* backend, const Window* window, const Camera* camera, Scene* scene) {

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    // ImGuizmo::BeginFrame();

    ImGui::Begin("Viewport");
    ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
    ImGui::Image((ImTextureID)backend->viewport_desc_sets[backend->current_frame_i], ImVec2{viewport_panel_size.x, viewport_panel_size.y});
    ImGui::End();

    editor->viewport_width  = viewport_panel_size.x;
    editor->viewport_height = viewport_panel_size.y;

    bool yeah = true;
    ImGui::ShowDemoWindow(&yeah);

    editor->imgui_io = &ImGui::GetIO();

    ImGui::Render();
}

void editor_update(Editor* editor, const VkBackend* backend, const Window* window, const Camera* camera, Scene* scene) {

    editor_update_viewport(editor, backend, window, camera, scene);
    return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    ImGuizmo::BeginFrame();

    bool show_window = true;

    ImGuiWindowFlags window_flags = 0;

    window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;

    ImGui::SetNextWindowSize(ImVec2{300.f, 0.0f}, ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2{0.f, 0.0f}, ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, static_cast<float>(window->height)}, ImVec2{FLT_MAX, static_cast<float>(window->height)});

    ImGui::Begin("Overview", &show_window, window_flags);

    // ui window
    float new_window_width = ImGui::GetWindowWidth();
    if (editor->window_width != new_window_width) {
        editor->ui_resized   = true;
        editor->window_width = new_window_width;
    }

    ImGui::Text("Frame time: %.3f ms (%.1f FPS)", backend->stats.frame_time, 1000.f / backend->stats.frame_time);
    ImGui::Text("Host buffer recording: %.3f us", backend->stats.draw_time);

    ImGui::BeginChild("Scene");
    for (size_t i = 0; i < scene->entities.size(); i++) {
        Entity* entity = &scene->entities[i];

        bool is_selected = (i == editor->selected_entity);
        if (ImGui::Selectable(entity->name.c_str(), is_selected)) {
            editor->selected_entity = i;
        }
    }
    ImGui::EndChild();
    ImGui::End();

    static int           movement_plane = 0; // 0: XZ, 1: XY, 2: YZ
    constexpr std::array plane_labels   = {"XZ Plane", "XY Plane", "YZ Plane"};

    if (editor->selected_entity >= 0) {

        ImGui::SetNextWindowSize(ImVec2{300.f, 0.0f}, ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2{window->width - 300.f, 0.0f}, ImGuiCond_Once);
        ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, static_cast<float>(window->height)}, ImVec2{FLT_MAX, static_cast<float>(window->height)});
        ImGui::Begin("Entity Properties", &show_window, window_flags);
        Entity* entity = &scene->entities[editor->selected_entity];

        ImGui::Text("%s", entity->name.c_str());
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float drag_speed = 0.1f;
            ImGui::PushItemWidth(150);
            // ImGui::DragFloat3("Position", &entity->pos.x, drag_speed);

            // Speed control for fine/coarse adjustment
            ImGui::SameLine();
            if (ImGui::Button("Fine")) {
                drag_speed = 0.01f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Coarse")) {
                drag_speed = 1.0f;
            }
            ImGui::Combo("Movement Plane", &movement_plane, plane_labels.data(), plane_labels.size());

            // Visual movement plane indicator
            //    ImGui::BeginChild("MovementPlane", ImVec2(200, 200), true);
            /*
            ImVec2 center = ImGui::GetWindowPos();
            center.x += 100;
            center.y += 100;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            */

            // Draw movement plane visualization
            /*switch (movement_plane) {
            case 0: // XZ
                draw_list->AddRect(ImVec2(center.x - 50, center.y - 50), ImVec2(center.x + 50, center.y + 50), IM_COL32(255, 255, 0, 255));
                // draw_list->u draw_list->AddText(ImVec2(center.x - 40, center.y - 40), IM_COL32(255, 255, 255, 255), "X");
                // draw_list->AddText(ImVec2(center.x + 30, center.y + 30), IM_COL32(255, 255, 255, 255), "Z");
                break;
            case 1: // XY
                    // Similar visualizations for other planes...
                break;
            case 2: // YZ
                break;
            default: {
                DEBUG_PRINT("Unreachable");
                exit(1);
            }
        }*/
        }
        // ImGui::EndChild();

        /*if (ImGui::IsWindowFocused()) {
            float moveSpeed = ImGui::GetIO().KeyShift ? 10.0f : 1.0f;

            switch (movement_plane) {
            case 0: // XZ plane
                if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                    scene->velocity.x -= moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
                    scene->velocity.x += moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
                    scene->velocity.z -= moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
                    scene->velocity.z += moveSpeed;
                break;
            case 1: // XY plane
                if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                    scene->velocity.x -= moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
                    scene->velocity.x += moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
                    scene->velocity.y += moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
                    scene->velocity.y -= moveSpeed;
                break;
            case 2: // YZ plane
                if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                    scene->velocity.z -= moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
                    scene->velocity.z += moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
                    scene->velocity.y += moveSpeed;
                if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
                    scene->velocity.y -= moveSpeed;
                break;
            }
            if (scene->velocity != glm::vec3(0)) {
                scene_request_update(scene);
            }
        }*/
        static float snapValue = 1.0f;
        /*if (ImGui::CollapsingHeader("Snapping")) {
            ImGui::DragFloat("Grid Size", &snapValue, 0.1f, 0.1f, 10.0f);
            if (ImGui::Button("Snap to Grid")) {
                entity->pos.x = round(entity->pos.x / snapValue) * snapValue;
                entity->pos.y = round(entity->pos.y / snapValue) * snapValue;
                entity->pos.z = round(entity->pos.z / snapValue) * snapValue;
            }
        }*/

        ImGui::End();
    }

    ImGui::Begin("Viewport");

    ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
    ImGui::Image((ImTextureID)backend->viewport_desc_sets[backend->current_frame_i], ImVec2{viewport_panel_size.x, viewport_panel_size.y});

    // Gizmos
    if (editor->selected_entity >= 0) {

        Entity* entity = &scene->entities[editor->selected_entity];

        if (ImGuizmo::IsUsing()) {
            scene_request_update(scene);
        }
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        float window_width  = (float)ImGui::GetWindowWidth();
        float window_height = (float)ImGui::GetWindowHeight();
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, window_width, window_height);
        ImGuizmo::Manipulate(glm::value_ptr(camera->view), glm::value_ptr(camera->proj), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::LOCAL,
                             glm::value_ptr(entity->transform));
    }
    ImGui::End();

    /*
    ImGuiTreeNodeFlags base_tree_flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNode("Scene")) {
        for (size_t i = 0; i < scene->entities.size(); i++) {
            // adding since all items are leaf nodes. will be different when adding nested nodes
            ImGuiTreeNodeFlags node_flags = base_tree_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            /*
            if (i == editor->selected_entity) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }
            #1#
            // ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, "%s", scene->entities[i].name.c_str());
            ImGui::PushID(i);
            if (ImGui::TreeNode(scene->entities[i].name.c_str())) {
                ImGui::Text("Position");
                ImGui::InputFloat("X", &scene->entities[i].pos.x);
                ImGui::InputFloat("Y", &scene->entities[i].pos.y);
                ImGui::InputFloat("Z", &scene->entities[i].pos.z);

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    editor->selected_entity = i;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();

    }*/

    // ImGui::End();
    // editor->imgui_draw_data = ImGui::GetDrawData();
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
// void editor_resize_callback(Editor* editor, int width, int height) { editor->ui_resized = true; }
