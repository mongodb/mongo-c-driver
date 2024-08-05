if (MONGO_FUZZ)

add_executable(fuzz_test_init_from_json
   fuzz_test_init_from_json.c)
target_include_directories(fuzz_test_init_from_json PRIVATE "src" "${PROJECT_BINARY_DIR}/src/")
target_link_libraries(fuzz_test_init_from_json ${bson_libs})
set_target_properties(fuzz_test_init_from_json PROPERTIES LINK_FLAGS ${MONGO_FUZZ_ENGINE})

endif ()
