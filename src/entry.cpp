#include "core/engine.h"

int main() {
    Engine engine{};
    engine_init(&engine, EngineMode::EDIT);

    engine_run(&engine);

    engine_deinit(&engine);
}
