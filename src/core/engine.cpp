#include "engine.h"
#include "core_options.h"
#include "fmt/os.h"

#include <GLFW/glfw3.h>
#include <array>
#include <core/camera.h>
#include <core/scene.h>
#include <core/ui.h>
#include <core/window.h>
#include <fstream>
#include <glm/vec3.hpp>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>
#include <glslang/Public/ResourceLimits.h>

#include <glslang/SPIRV/GlslangToSpv.h>
#include <iostream>
#include <vk_backend/vk_backend.h>

static Engine* active_engine = nullptr;

int compile_shader(const std::filesystem::path& file_path, glslang_stage_t stage,
                   std::vector<uint32_t>* output) {

    std::ifstream     file(file_path, std::ios::ate | std::ios::binary);
    size_t            file_size = std::filesystem::file_size(file_path);
    std::vector<char> file_buf(file_size);

    file.seekg(0);
    file.read(file_buf.data(), file_size);
    file.close();

    glslang_resource_t default_resource{};

    const glslang_input_t glslang_input = {
        .language                          = GLSLANG_SOURCE_GLSL,
        .stage                             = stage,
        .client                            = GLSLANG_CLIENT_VULKAN,
        .client_version                    = GLSLANG_TARGET_VULKAN_1_3,
        .target_language                   = GLSLANG_TARGET_SPV,
        .target_language_version           = GLSLANG_TARGET_SPV_1_3,
        .code                              = file_buf.data(),
        .default_version                   = 100,
        .default_profile                   = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible                = false,
        .messages                          = GLSLANG_MSG_DEFAULT_BIT,
        .resource                          = &glslang::,
    };

    glslang_shader_t* shader = glslang_shader_create(&glslang_input);

    if (!glslang_shader_preprocess(shader, &glslang_input)) {
        std::cerr << "GLSL Preprocessing Failed\n";
        std::cerr << glslang_shader_get_info_log(shader) << "\n";
        std::cerr << glslang_shader_get_info_debug_log(shader) << "\n";
        return -1;
    }

    if (!glslang_shader_parse(shader, &glslang_input)) {
        std::cerr << "GLSL Parsing Failed\n";
        std::cerr << glslang_shader_get_info_log(shader) << "\n";
        std::cerr << glslang_shader_get_info_debug_log(shader) << "\n";
        return -1;
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    int msgs = GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT;

    if (!glslang_program_link(program, msgs)) {
        std::cerr << "GLSL Linking Failed\n";
        std::cerr << glslang_program_get_info_log(program) << "\n";
        std::cerr << glslang_program_get_info_debug_log(program) << "\n";
        return -1;
    }

    glslang_program_SPIRV_generate(program, stage);

    output->resize(glslang_program_SPIRV_get_size(program));
    glslang_program_SPIRV_get(program, reinterpret_cast<unsigned int*>(output->data()));
    return 1;
}

void init_engine(Engine* engine) {
    assert(active_engine == nullptr);
    active_engine = engine;

    constexpr glm::vec3 init_cam_pos = {0, -1, -8};

    init_window(&engine->window, core_opts::initial_width, core_opts::initial_height,
                "Triangle Machine");

    init_camera(&engine->camera, &engine->window, init_cam_pos);

    const VkInstance   instance = create_vk_instance("triangle machine", "my engine");
    const VkSurfaceKHR surface  = get_vulkan_surface(&engine->window, instance);

    init_backend(&engine->backend, instance, surface, engine->window.width, engine->window.height);
    init_ui(&engine->ui, &engine->backend, engine->window.glfw_window);

    // std::ifstream vert_file("../shaders/vertex/indexed_draw.vert.glsl.spv",
    // std::ios::ate | std::ios::binary);
    // size_t        vert_file_size =
    // std::filesystem::file_size("../shaders/vertex/indexed_draw.vert.glsl.spv");
    // std::vector<uint8_t> vert_buffer(vert_file_size);

    // vert_file.seekg(0);
    // vert_file.read(reinterpret_cast<char*>(vert_buffer.data()), vert_file_size);
    // vert_file.close();

    // std::ifstream frag_file("../shaders/fragment/simple_lighting.frag.glsl.spv",
    // std::ios::ate | std::ios::binary);
    // size_t        frag_file_size =
    // std::filesystem::file_size("../shaders/fragment/simple_lighting.frag.glsl.spv");
    // std::vector<uint8_t> frag_buffer(frag_file_size);

    // frag_file.seekg(0);
    // frag_file.read(reinterpret_cast<char*>(frag_buffer.data()), frag_file_size);
    // frag_file.close();

    std::vector<uint32_t> vert_spv;
    compile_shader("../shaders/vertex/indexed_draw.vert.glsl", GLSLANG_STAGE_VERTEX, &vert_spv);
    std::vector<uint32_t> frag_spv;
    compile_shader("../shaders/fragment/simple_lighting.frag.glsl", GLSLANG_STAGE_FRAGMENT,
                   &frag_spv);

    create_pipeline(&engine->backend, vert_spv, frag_spv);

    /*
    std::array gltf_paths = {
        "../assets/gltf/main_sponza/pkg_a_Curtains/NewSponza_Curtains_glTF.gltf",
        "../assets/gltf/main_sponza/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf",
        "../assets/gltf/main_sponza/pkg_c_trees/NewSponza_CypressTree_glTF.gltf",
        "../assets/gltf/main_sponza/Main.1_Sponza/NewSponza_Main_glTF_002.gltf"};
    */

    std::array gltf_paths = {"../assets/glb/porsche.glb"};
    load_scene(&engine->scene, &engine->backend, gltf_paths);

    register_key_callback(&engine->window, [=](int key, int scancode, int action, int mods) {
        camera_key_callback(&engine->camera, key, scancode, action, mods);
        scene_key_callback(&engine->scene, key, action);
    });

    register_cursor_callback(&engine->window, [=](double x_pos, double y_pos) {
        camera_cursor_callback(&engine->camera, x_pos, y_pos);
    });
}

void run_engine(Engine* engine) {

    while (!glfwWindowShouldClose(engine->window.glfw_window)) {
        glfwPollEvents();

        SceneData scene_data =
            update_camera(&engine->camera, engine->window.width, engine->window.height);
        update_scene(&engine->scene);

        update_ui(&engine->backend);
        draw(&engine->backend, engine->scene.entities, &scene_data);
    }
}

void deinit_engine(Engine* engine) {
    finish_pending_vk_work(&engine->backend);

    deinit_ui(&engine->ui);
    deinit_window(&engine->window);
    deinit_backend(&engine->backend);
}