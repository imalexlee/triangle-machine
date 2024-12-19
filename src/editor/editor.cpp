#include "editor.h"

#include "ImGuizmo.h"
#include "core/engine.h"
#include "graphics/camera/camera.h"
#include "scene/scene.h"
#include "system/platform/window.h"

#include "nlohmann/json.hpp"
#include <glm/gtx/matrix_decompose.hpp>

#include <ImGuiFileDialog.h>
#include <imconfig.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <iostream>

static void end_ui(Editor* editor);
static void begin_ui();

void editor_init(Editor* editor, Renderer* backend, Camera* camera, GLFWwindow* window) {
    editor->imgui_ctx = ImGui::CreateContext();

    editor->imgui_io              = &ImGui::GetIO();
    editor->imgui_io->ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_IsSRGB;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui::StyleColorsDark();

    editor->imgui_style = &ImGui::GetStyle();
    // gamma corrected
    editor->imgui_style->Colors[ImGuiCol_WindowBg]   = ImVec4(pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), pow(56.f / 255, 2.2), 1.0);
    editor->imgui_style->Colors[ImGuiCol_WindowBg].w = 1.0f;

    editor->gizmo_op = ImGuizmo::TRANSLATE;

    namespace fs = std::filesystem;
    if (!fs::is_directory(editor->app_data_dir) || !fs::exists(editor->app_data_dir)) {
        editor->app_data_dir_exists = fs::create_directory(editor->app_data_dir);
    } else {
        editor->app_data_dir_exists = true;
    }
    for (const auto& path : std::filesystem::directory_iterator(editor->app_data_dir)) {
        if (path.is_regular_file()) {
            editor->app_data_dir_contains_files = true;
            break;
        }
    }

    renderer_create_imgui_resources(backend);

    camera_register_update_callback(camera, [=] {
        camera->proj = glm::perspective(glm::radians(45.f), static_cast<float>(editor->viewport_width) / static_cast<float>(editor->viewport_height),
                                        10000.0f, 0.1f);
        //  cam->proj[1][1] *= -1; // correcting for Vulkans inverted Y coordinate
    });
}

static void begin_ui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    // GetID calculates a hash. seems unnecessary for now so I'm going to use 0
    // ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGuiID dockspace_id = 0;
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());
}

static void end_ui(Editor* editor) {
    editor->imgui_io = &ImGui::GetIO();
    ImGui::End();
    ImGui::Render();
}

glm::mat4 calculateTransformDifference(const glm::mat4& sourceTransform, const glm::mat4& targetTransform) {
    glm::vec3 sourceTranslation, sourceScale;
    glm::quat sourceRotation;
    glm::vec3 sourceSkew;
    glm::vec4 sourcePerspective;
    glm::decompose(sourceTransform, sourceScale, sourceRotation, sourceTranslation, sourceSkew, sourcePerspective);

    glm::vec3 targetTranslation, targetScale;
    glm::quat targetRotation;
    glm::vec3 targetSkew;
    glm::vec4 targetPerspective;
    glm::decompose(targetTransform, targetScale, targetRotation, targetTranslation, targetSkew, targetPerspective);

    glm::vec3 translationDiff = targetTranslation - sourceTranslation;
    glm::vec3 scaleDiff       = targetScale / sourceScale;
    glm::quat rotationDiff    = glm::inverse(sourceRotation) * targetRotation;

    glm::mat4 diffTransform = glm::mat4(1.0f);

    // go to origin first
    diffTransform *= glm::translate(diffTransform, targetTranslation);
    diffTransform *= glm::mat4_cast(rotationDiff);
    diffTransform = glm::scale(diffTransform, scaleDiff);
    diffTransform *= glm::translate(glm::mat4(1.0f), -targetTranslation);
    diffTransform = glm::translate(diffTransform, translationDiff);

    return diffTransform;
}
using namespace std::chrono;
static auto start_time = high_resolution_clock::now();

void update_viewport(Editor* editor, const Renderer* backend, const Window* window, Camera* camera, Scene* scene) {
    auto  time_duration = duration_cast<duration<float>>(high_resolution_clock::now() - start_time);
    float time_elapsed  = time_duration.count();

    ImGui::Begin("Viewport");

    ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
    ImGui::Image(reinterpret_cast<ImTextureID>(backend->viewport_desc_sets[backend->current_frame_i]),
                 ImVec2{viewport_panel_size.x, viewport_panel_size.y});

    editor->viewport_width  = viewport_panel_size.x;
    editor->viewport_height = viewport_panel_size.y;

    if (ImGui::IsKeyDown(ImGuiKey_MouseRight)) {
        ImVec2 mouse_delta = editor->imgui_io->MouseDelta;
        // fast zoom: Shift + right mouse button
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
            camera_zoom(camera, -mouse_delta.y * time_elapsed * 15);
        } else {
            camera_orbit(camera, mouse_delta.y * -time_elapsed * 4, mouse_delta.x * -time_elapsed * 4);
        }
    }
    if (ImGui::IsKeyDown(ImGuiKey_MouseMiddle)) {
        ImVec2 mouse_delta = editor->imgui_io->MouseDelta;
        camera_pan(camera, mouse_delta.x * time_elapsed, mouse_delta.y * time_elapsed);
        // std::cout << camera->position.x << " " << camera->position.y << " " << camera->position.z << std::endl;
        // std::cout << camera->look_at.x << " " << camera->look_at.y << " " << camera->look_at.z << '\n' << std::endl;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_MouseLeft, false) && !ImGuizmo::IsOver()) {

        // only register clicks within viewport
        glm::vec2 bounds_horiz = {ImGui::GetWindowPos().x, ImGui::GetWindowPos().x + editor->viewport_width};
        glm::vec2 bounds_vert  = {ImGui::GetWindowPos().y, ImGui::GetWindowPos().y + editor->viewport_height};
        float     mouse_x      = editor->imgui_io->MousePos.x;
        float     mouse_y      = editor->imgui_io->MousePos.y;

        if (mouse_x >= bounds_horiz[0] && mouse_x <= bounds_horiz[1] && mouse_y >= bounds_vert[0] && mouse_y <= bounds_vert[1]) {
            ImVec2 viewport_window_size = ImGui::GetWindowSize();

            // relative percentage of mouse in viewport. for x: 0.0 for left side. 1.0 for right side
            float x_rel_percent = (mouse_x - bounds_horiz[0]) / viewport_window_size.x;
            float y_rel_percent = (mouse_y - bounds_vert[0]) / viewport_window_size.y;

            int32_t offset_x = window->width * x_rel_percent;
            int32_t offset_y = window->height * y_rel_percent;

            uint16_t ent_id        = renderer_entity_id_at_pos(backend, offset_x, offset_y);
            scene->selected_entity = ent_id > 0 ? ent_id - 1 : -1;
        }
    }

    // scroll wheel zoom
    if (editor->imgui_io->MouseWheel != 0) {
        camera_zoom(camera, editor->imgui_io->MouseWheel * time_elapsed * 20);
    }

    if (scene->selected_entity >= 0) {

        if (ImGuizmo::IsUsing()) {
            Entity* entity    = &scene->entities[scene->selected_entity];
            entity->transform = calculateTransformDifference(editor->base_gizmo_transforms[scene->selected_entity],
                                                             editor->curr_gizmo_transforms[scene->selected_entity]);

            // pull the transform out of the average mesh position
            glm::vec3 translation, scale;
            glm::quat rotation;
            glm::vec3 skew;
            glm::vec4 perspective;

            glm::decompose(editor->curr_gizmo_transforms[scene->selected_entity], scale, rotation, translation, skew, perspective);
            entity->orig_pos = translation;

            scene_request_update(scene);
        }
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 window_pos  = ImGui::GetWindowPos();
        ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);

        glm::mat4 view = camera->view;
        glm::mat4 proj = camera->proj;
        proj[1][1] *= -1;

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), static_cast<ImGuizmo::OPERATION>(editor->gizmo_op),
                             static_cast<ImGuizmo::MODE>(editor->gizmo_mode), glm::value_ptr(editor->curr_gizmo_transforms[scene->selected_entity]));
    }
    start_time = high_resolution_clock::now();
}

glm::mat4 calculateAverageTransform(const std::vector<glm::mat4>& transformations) {
    if (transformations.empty()) {
        return glm::mat4(1.0f);
    }

    // Calculate the average transformation matrix
    glm::mat4 averageTransform(1.0f);

    for (const auto& transform : transformations) {
        glm::vec3 translation, scale;
        glm::quat rotation;
        glm::vec3 skew;
        glm::vec4 perspective;

        glm::decompose(transform, scale, rotation, translation, skew, perspective);

        averageTransform = glm::translate(averageTransform, translation / static_cast<float>(transformations.size()));
        // Simple averaging of rotation may not produce correct results
    }

    return averageTransform;
}

void update_file_menu(Editor* editor, Renderer* backend, Scene* scene) {
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    static bool new_selected = false;
    if (ImGui::MenuItem("New", "Ctrl+N", false, editor->app_data_dir_exists)) {
        new_selected = true;
    }
    static bool open_selected = false;
    if (ImGui::MenuItem("Open", "Ctrl+O", false, editor->app_data_dir_contains_files)) {
        open_selected = true;
    }
    ImGui::PopItemFlag();
    static bool save_selected = false;
    if (ImGui::MenuItem("Save", "Ctrl+O", false, !editor->curr_scene_path.empty())) {
        save_selected = true;
    }
    if (ImGui::MenuItem("Quit", "Alt+F4")) {
        editor->quit = true;
    }

    // handle selections
    if (new_selected || open_selected) {
        IGFD::FileDialogConfig config;
        config.path  = editor->app_data_dir.string();
        config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", new_selected ? "Create New File" : "Open File", ".json", config);

        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string file_path    = ImGuiFileDialog::Instance()->GetFilePathName();
                std::string current_path = ImGuiFileDialog::Instance()->GetCurrentPath();
                if (new_selected) {
                    std::ofstream new_file(file_path);
                    using namespace nlohmann::literals;
                    new_file << R"+({"scene":{"entities":[]}})+"_json;
                    new_file.close();
                }
                scene_open(scene, backend, file_path);
                editor->curr_scene_path = file_path;
                new_selected            = false;
                open_selected           = false;

                // set gizmo transform at average location of all mesh transforms of a given entity
                // so the center
                for (auto& entity : scene->entities) {
                    std::vector<glm::mat4> transforms;
                    for (const auto opaque_obj : entity.opaque_objs) {
                        transforms.push_back(opaque_obj.mesh_data.local_transform);
                    }
                    for (const auto trans_obj : entity.transparent_objs) {
                        transforms.push_back(trans_obj.mesh_data.local_transform);
                    }
                    glm::mat4 avg_transform = calculateAverageTransform(transforms);
                    editor->base_gizmo_transforms.push_back(avg_transform);
                    editor->curr_gizmo_transforms.push_back(entity.transform * avg_transform);

                    // pull the transform out of the average mesh position
                    glm::vec3 translation, scale;
                    glm::quat rotation;
                    glm::vec3 skew;
                    glm::vec4 perspective;

                    glm::decompose(entity.transform * avg_transform, scale, rotation, translation, skew, perspective);
                    entity.orig_pos = translation;
                }
            } else {
                // cancel was clicked
                new_selected  = false;
                open_selected = false;
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (save_selected) {
        assert(!editor->curr_scene_path.empty());
        scene_save(scene, editor->curr_scene_path);
    }
}

void update_scene_overview(Editor* editor, Renderer* backend, const Window* window, const Camera* camera, Scene* scene) {

    static bool show_window = true;
    ImGui::Begin("Overview", &show_window, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            update_file_menu(editor, backend, scene);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("Frame time: %.3f ms (%.1f FPS)", backend->stats.frame_time, 1000.f / backend->stats.frame_time);
    ImGui::Text("Host buffer recording: %.3f us", backend->stats.draw_time);
    if (ImGui::Button("Recompile Fragment Shader")) {
        // TODO: don't hardcode the index
        renderer_recompile_frag_shader(backend, 1);
    }
    ImGuiTreeNodeFlags base_tree_flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanAvailWidth;
    static bool add_entity_pressed = false;
    if (!editor->curr_scene_path.empty() && (ImGui::Button("Add Entity +") || add_entity_pressed)) {
        add_entity_pressed = true;

        IGFD::FileDialogConfig config;
        config.path  = editor->app_data_dir.string();
        config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Add Entity", ".glb,.gltf", config);
        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string gltf_path = ImGuiFileDialog::Instance()->GetFilePathName();

                scene_load_gltf_path(scene, backend, gltf_path);
                uint32_t new_entity_idx = scene->entities.size() - 1;

                std::vector<glm::mat4> transforms;
                for (const auto opaque_obj : scene->entities[new_entity_idx].opaque_objs) {
                    transforms.push_back(opaque_obj.mesh_data.local_transform);
                }
                for (const auto trans_obj : scene->entities[new_entity_idx].transparent_objs) {
                    transforms.push_back(trans_obj.mesh_data.local_transform);
                }
                glm::mat4 avg_transform = calculateAverageTransform(transforms);
                editor->base_gizmo_transforms.push_back(avg_transform);
                editor->curr_gizmo_transforms.push_back(avg_transform);

                // pull the transform out of the average mesh position
                glm::vec3 translation, scale;
                glm::quat rotation;
                glm::vec3 skew;
                glm::vec4 perspective;

                glm::decompose(avg_transform, scale, rotation, translation, skew, perspective);
                scene->entities[new_entity_idx].orig_pos = translation;

                add_entity_pressed = false;
            } else {
                add_entity_pressed = false;
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGui::TreeNode("Scene")) {

        for (size_t i = 0; i < scene->entities.size(); i++) {
            // adding since all items are leaf nodes. will be different when adding nested nodes
            ImGuiTreeNodeFlags node_flags = base_tree_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            if (i == scene->selected_entity) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }
            ImGui::TreeNodeEx(reinterpret_cast<void*>(i), node_flags, "%s", scene->entities[i].name.c_str());

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                // toggle off if already selected
                scene->selected_entity = i == scene->selected_entity ? -1 : i;
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

void update_entity_viewer(Editor* editor, Scene* scene) {
    // if (scene->selected_entity < 0) {
    //     return;
    // }
    // static bool show_window = true;
    // if (!show_window) {
    //     scene->selected_entity = -1;
    //     show_window            = true;
    //     return;
    // }
    ImGui::Begin("Entity Details");
    if (scene->selected_entity >= 0) {
        ImGui::Text(scene->entities[scene->selected_entity].name.c_str());
        ImGui::Text("Gizmo Mode");
        ImGui::RadioButton("Local", &editor->gizmo_mode, ImGuizmo::MODE::LOCAL);
        ImGui::SameLine();
        ImGui::RadioButton("World", &editor->gizmo_mode, ImGuizmo::MODE::WORLD);

        ImGui::Text("Gizmo Operation");
        ImGui::RadioButton("Translate", &editor->gizmo_op, ImGuizmo::TRANSLATE);
        ImGui::SameLine();
        ImGui::RadioButton("Rotate", &editor->gizmo_op, ImGuizmo::ROTATE);
        ImGui::RadioButton("Scale", &editor->gizmo_op, ImGuizmo::SCALE);

        float   translation[3], rotation[3], scale[3];
        Entity* curr_entity = &scene->entities[scene->selected_entity];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(editor->curr_gizmo_transforms[scene->selected_entity]), translation, rotation, scale);
        ImGui::Text("Entity Transformation");
        ImGui::InputFloat3("Tr", translation, "%.3f");
        ImGui::InputFloat3("Rt", rotation, "%.3f");
        ImGui::InputFloat3("Sc", scale, "%.3f");
        // ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale,
        // glm::value_ptr(editor->curr_gizmo_transforms[scene->selected_entity]));
    } else {
        ImGui::Text("No Entity Selected");
    }

    ImGui::End();
}

void editor_update(Editor* editor, Renderer* backend, const Window* window, Camera* camera, Scene* scene) {
    begin_ui();
    update_viewport(editor, backend, window, camera, scene);
    update_scene_overview(editor, backend, window, camera, scene);
    update_entity_viewer(editor, scene);
    end_ui(editor);
}

void editor_deinit(const Editor* editor) {

    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplVulkan_Shutdown();

    ImGui::DestroyContext(editor->imgui_ctx);
}
