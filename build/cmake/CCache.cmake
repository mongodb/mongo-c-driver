#[[
    This module enables Ccache support by inserting a ccache executable as
    the compiler launcher for C and C++ if there is a ccache executable availbale
    on the system.

    Ccache support will be automatically enabled if it is found on the system.
    Ccache can be forced on or off by setting the MONGO_USE_CCACHE CMake option to
    ON or OFF.
]]

# Find and enable ccache for compiling
find_program (CCACHE_EXECUTABLE ccache)

if (CCACHE_EXECUTABLE)
    message (STATUS "Compiling with Ccache enabled. Disable by setting MONGO_USE_CCACHE to OFF")
    set (CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
    set (CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
endif ()
