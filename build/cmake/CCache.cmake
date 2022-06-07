#[[
    This module enables CCache support by inserting a ccache executable as
    the compiler launcher for C and C++ if there is a ccache executable availbale
    on the system.

    CCache support will be automatically enabled if it is found on the system.
    CCache can be forced on or off by setting the MONGO_USE_CCACHE CMake option to
    ON or OFF.
]]

# Find and enable ccache for compiling if not already found.
if (NOT DEFINED MONGO_USE_CCACHE)
    find_program (CCACHE_EXECUTABLE ccache)
    if (CCACHE_EXECUTABLE)
        message (STATUS "Found ccache: ${CCACHE_EXECUTABLE}")

        execute_process (
            COMMAND ${CCACHE_EXECUTABLE} --version | perl -ne "print $1 if /^ccache version (.+)$/"
            OUTPUT_VARIABLE CCACHE_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # Assume `ccache --version` mentions a simple version string, e.g. "1.2.3".
        # Permit patch number to be omitted, e.g. "1.2".
        set (SIMPLE_SEMVER_REGEX "([0-9]+)\.([0-9]+)(\.([0-9]+))?")
        string (REGEX MATCH "${SIMPLE_SEMVER_REGEX}" CCACHE_VERSION ${CCACHE_VERSION})

        # Avoid spurious "ccache.conf: No such file or directory" errors due to ccache being invoked in parallel, which was patched in ccache version 3.4.3.
        if (${CCACHE_VERSION} VERSION_LESS 3.4.3)
            message (STATUS "Detected ccache version ${CCACHE_VERSION} is less than 3.4.3, which may lead to spurious failures when run in parallel. See https://github.com/ccache/ccache/issues/260 for more information.")
            message (STATUS "Compiling with CCache disabled. Enable by setting MONGO_USE_CCACHE to ON")
            option (MONGO_USE_CCACHE "Use CCache when compiling" OFF)
        else ()
            message (STATUS "Compiling with CCache enabled. Disable by setting MONGO_USE_CCACHE to OFF")
            option (MONGO_USE_CCACHE "Use CCache when compiling" ON)
        endif ()
    endif (CCACHE_EXECUTABLE)
endif (NOT DEFINED MONGO_USE_CCACHE)

if (MONGO_USE_CCACHE)
    set (CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
    set (CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
endif ()
