include (CheckCSourceCompiles)
include (CMakePushCheckState)
include (MongoSettings)

mongo_setting(
  MONGO_FUZZ "Whether fuzz testing is enabled"
  TYPE BOOL
  DEFAULT FALSE
)

if (MONGO_FUZZ)
set(CMAKE_C_FLAGS "-fsanitize=undefined -fsanitize=address -fsanitize=fuzzer-no-link" CACHE STRING "Custom comp" FORCE)
set(MONGO_FUZZ_ENGINE -fsanitize=fuzzer)
endif ()