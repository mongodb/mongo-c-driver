add_executable(fuzz_test_init_from_json EXCLUDE_FROM_ALL
   fuzz_test_init_from_json.c)
target_link_libraries(fuzz_test_init_from_json PRIVATE bson_static)
set_property(TARGET fuzz_test_init_from_json APPEND PROPERTY LINK_OPTIONS -fsanitize=fuzzer)

add_executable(fuzz_test_validate EXCLUDE_FROM_ALL
   fuzz_test_validate.c)
target_link_libraries(fuzz_test_validate PRIVATE bson_static)
set_property(TARGET fuzz_test_validate APPEND PROPERTY LINK_OPTIONS -fsanitize=fuzzer)
