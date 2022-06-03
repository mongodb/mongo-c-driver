include (CheckCSourceCompiles)
include (CMakePushCheckState)

## Directly control the options passed to -fsanitize
set (MONGO_SANITIZE "" CACHE STRING "Semicolon/comma-separated list of sanitizers to apply when building")

# Replace commas with semicolons for the genex
string(REPLACE ";" "," _sanitize "${MONGO_SANITIZE}")

if (_sanitize)
    string (MAKE_C_IDENTIFIER "HAVE_SANITIZE_${_sanitize}" ident)
    string (TOUPPER "${ident}" varname)
    set (flag "-fsanitize=${_sanitize}")

    cmake_push_check_state ()
        set (CMAKE_REQUIRED_FLAGS "${flag}")
        set (CMAKE_REQUIRED_LIBRARIES "${flag}")
        check_c_source_compiles ([[
            #include <stdio.h>

            int main (void) {
                puts ("Hello, world!");
                return 0;
            }
        ]] "${varname}")
    cmake_pop_check_state ()

    if (NOT "${${varname}}")
        message (SEND_ERROR "Requested sanitizer option '${flag}' is not supported by the compiler+linker")
    else ()
        message (STATUS "Building with ${flag}")
        add_compile_options ("${flag}")
        link_libraries ("${flag}")
    endif ()
endif ()
