#include "core/engine.h"

int main() {
    Engine engine{};

    EngineFeatures features = EngineFeatures::DEBUG_GRID;

    engine_enable_features(&engine, features);

    engine_init(&engine, EngineMode::EDIT);

    // scene_open(&engine.scene, &engine.renderer, "app_data/sponza.json");
    // const std::string gltf_path = "../assets/glb/monkey.glb";
    // scene_load_gltf_path(&engine.scene, &engine.renderer, gltf_path);

    while (engine_is_alive(&engine)) {
        engine_begin_frame(&engine);

        engine_end_frame(&engine);
    }

    engine_deinit(&engine);
}
