#include "core/engine.h"

int main() {
    Engine engine{};

    EngineFeatures features = EngineFeatures::DEBUG_GRID;

    engine_enable_features(&engine, features);

    engine_init(&engine, EngineMode::EDIT);

    engine_run(&engine);

    engine_deinit(&engine);
}
