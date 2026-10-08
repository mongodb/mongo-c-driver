
set (MONGOC_ENABLE_DEBUG_ASSERTIONS 0)

if (ENABLE_DEBUG_ASSERTIONS)
    set (MONGOC_ENABLE_DEBUG_ASSERTIONS 1)
endif ()

configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/src/common-config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/common-config.h"
)

add_library(mongo-mlib INTERFACE)
add_library(mongo::mlib ALIAS mongo-mlib)
target_include_directories(mongo-mlib INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>)
set_property(TARGET mongo-mlib PROPERTY EXPORT_NAME mongo::mlib)

# The mlib allocator lives alone in an object file library, which must be adopted by exactly one
# downstream consumer in the final runtime program
add_library(mongo-mlib-allocator OBJECT src/mlib/alloc.c)
add_library(mongo::mlib::allocator ALIAS mongo-mlib-allocator)
target_link_libraries(mongo-mlib-allocator PRIVATE mongo::mlib)
target_compile_options(mongo-mlib-allocator PRIVATE ${mongoc-warning-options})
set_target_properties(mongo-mlib-allocator PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    C_VISIBILITY_PRESET hidden
    )

if(BUILD_TESTING)
    # These special test cases are written specifically to ensure we have predictable
    # and correct external-linkage object identity of the mlib allocator object.
    function(_mongo_add_allocator_test suffix owner)
        set(test_target "mlib-alloc${suffix}-test")
        add_executable(${test_target}
            tests/test-mlib-alloc.c
            tests/test-mlib-alloc-peer.c
            )
        target_link_libraries(${test_target} PRIVATE mongo::mlib ${owner})
        target_compile_options(${test_target} PRIVATE ${mongoc-warning-options})
        set_target_properties(${test_target} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        if(CMAKE_CXX_COMPILER)
            target_sources(${test_target} PRIVATE tests/test-mlib-alloc.cpp)
            target_compile_features(${test_target} PRIVATE cxx_std_11)
            target_compile_definitions(${test_target} PRIVATE MLIB_TEST_CXX)
        endif()
        add_test(NAME "mongoc/mlib/alloc${suffix}" COMMAND ${test_target})
    endfunction()

    _mongo_add_allocator_test("" mongo::mlib::allocator)
    # Exercise owner exports and archive extraction without embedding another object.
    if(NOT USE_SYSTEM_LIBBSON)
        if(ENABLE_STATIC)
            _mongo_add_allocator_test("-static" bson::static)
        endif()
        if(ENABLE_SHARED)
            _mongo_add_allocator_test("-shared" bson::shared)
        endif()
    endif()
endif()

if(BUILD_TESTING AND CMAKE_CXX_COMPILER)
    add_executable(mlib-ckdint-test src/mlib/ckdint.test.cpp)
    set_target_properties(mlib-ckdint-test PROPERTIES
        COMPILE_FEATURES cxx_std_11
        LINK_LIBRARIES mongo::mlib
        # Enable -fPIC, required for some build configurations
        POSITION_INDEPENDENT_CODE TRUE
        )
    add_test(NAME mongoc/mlib/ckdint COMMAND mlib-ckdint-test)
    set_property(TEST mongoc/mlib/ckdint PROPERTY SKIP_REGULAR_EXPRESSION "@@ctest-skipped@@")
endif()
