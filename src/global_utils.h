#pragma once

// prints disappear
// when running in
// release mode
#ifndef NDEBUG
#define DEBUG_PRINT(message, ...) fprintf(stderr, message "\n" __VA_OPT__(, ) __VA_ARGS__)
#else
#define DEBUG_PRINT(message, ...)
#endif
