#include "core/engine.h"

int main() {
    Engine engine{};

    EngineFeatures features =  EngineFeatures::DEBUG_GRID;

    engine_enable_features(&engine, features);

    engine_init(&engine, EngineMode::EDIT);

    while (engine_is_alive(&engine)) {
        engine_begin_frame(&engine);

        engine_end_frame(&engine);
    }

    engine_deinit(&engine);
}
