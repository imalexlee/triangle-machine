#include "core/engine.h"
#include "fmt/base.h"
#include <array>
#include <iostream>

int main() {
  // Engine engine;
  // engine.init();

  // engine.start();
  uint64_t numbers[5] = {1, 2, 3, 4, 5};

  for (auto& num : numbers) {
    num = 6;
  }
  for (int num : numbers) {
    std::cout << "number: " << num << "\n";
  }
}
