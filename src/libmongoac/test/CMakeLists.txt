# Catch2 test suite requires C++17 or newer.
enable_language(CXX)

# Import (patched) Catch2 with CTest integration.
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include (FetchCatch2)
FetchContent_GetProperties (EP_Catch2)
list (APPEND CMAKE_MODULE_PATH "${ep_catch2_SOURCE_DIR}/extras")
include (Catch)

# Prefer the shared library when enabled (to validate symbol exports).
if (TARGET mongoac_shared)
  set (mongoac_target mongoac_shared)
else ()
  set (mongoac_target mongoac_static)
endif ()

# Support external BSON library when mongoac is configured standalone.
if (NOT TARGET bson::shared AND NOT TARGET bson::static)
  find_package(bson 2.0 REQUIRED)
endif ()

# Prefer the shared library when available.
if (TARGET bson::shared)
  set (bson_target bson::shared)
else ()
  set (bson_target bson::static)
endif ()

add_executable (test-libmongoac EXCLUDE_FROM_ALL)
target_compile_features (test-libmongoac PRIVATE cxx_std_17)
target_include_directories (test-libmongoac PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}") # <mongoac/test/...>
target_link_libraries (test-libmongoac PRIVATE "${mongoac_target}")

# Add to list of mongo-c-driver test executables.
if (TARGET mongo_c_driver_tests)
  add_dependencies (mongo_c_driver_tests test-libmongoac)
endif ()

# Avoid emitting warnings from bson headers.
target_link_libraries (test-libmongoac PRIVATE "$<LINK_ONLY:${bson_target}>")
target_include_directories (test-libmongoac
  SYSTEM PRIVATE "$<TARGET_PROPERTY:${bson_target},INTERFACE_INCLUDE_DIRECTORIES>"
)

file (GLOB_RECURSE mongoac_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file (GLOB_RECURSE mongoac_test_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hh")
target_sources (test-libmongoac
  PRIVATE
    ${mongoac_test_sources}
  PRIVATE
    FILE_SET HEADERS
      BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
      FILES ${mongoac_test_headers}
)
target_link_libraries (test-libmongoac PRIVATE Catch2::Catch2WithMain)

catch_discover_tests (test-libmongoac
  TEST_SPEC "[run-serial]"
  TEST_PREFIX "mongoac/"
  ADD_TAGS_AS_LABELS
  PROPERTIES
    RUN_SERIAL TRUE
)

catch_discover_tests (test-libmongoac
  TEST_SPEC "~[run-serial]"
  TEST_PREFIX "mongoac/"
  ADD_TAGS_AS_LABELS
)
