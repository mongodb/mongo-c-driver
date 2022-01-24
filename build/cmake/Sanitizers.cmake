## Directly control the options passed to -fsanitize
set (MONGO_SANITIZE "" CACHE STRING "Semicolon/comma-separated list of sanitizers to apply when building")

# Replace commas with semicolons for the genex
string(REPLACE ";" "," _sanitize "${MONGO_SANITIZE}")

if (_sanitize)
    message (STATUS "Building with -fsanitize=${_sanitize}")
    add_compile_options ("-fsanitize=${_sanitize}")
    link_libraries ("-fsanitize=${_sanitize}")
endif ()
