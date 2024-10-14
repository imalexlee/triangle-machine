#include "core/engine.h"

int main() {
    Engine engine{};
    engine_init(&engine);

    engine_run(&engine);

    engine_deinit(&engine);
}
