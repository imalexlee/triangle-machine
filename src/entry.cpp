#include "core/engine.h"

int main() {
    Engine engine;
    init_engine(&engine);

    run_engine(&engine);

    deinit_engine(&engine);
}
