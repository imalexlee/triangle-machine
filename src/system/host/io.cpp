#include "io.h"

#include <cassert>
#include <fstream>
#include <sstream>

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    assert(file.is_open());
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}
