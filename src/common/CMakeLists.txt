
set (MONGOC_ENABLE_DEBUG_ASSERTIONS 0)

if (ENABLE_DEBUG_ASSERTIONS)
    set (MONGOC_ENABLE_DEBUG_ASSERTIONS 1)
endif ()

configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/src/common-config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/common-config.h"
)
