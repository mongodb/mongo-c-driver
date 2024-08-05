set(CMAKE_C_FLAGS "-fsanitize=undefined -fsanitize=address -fsanitize=fuzzer-no-link" CACHE STRING "Custom comp" FORCE)
set(FUZZING_ENGINE "-fsanitize=fuzzer" CACHE STRING "Use libfuzzer" FORCE)
