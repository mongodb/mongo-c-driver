
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

# Keep allocator storage separate from the header-only mongo::mlib target. This
# OBJECT library compiles the allocator state owner without producing a standalone archive
# or shared library. For a set of linked runtime images intended to share allocator
# state, choose one owning library or executable; embedding another copy can create
# independent state. Consumers embedding allocator storage directly link the object
# target through the mongo::mlib::allocator alias.
add_library(mongo-mlib-allocator OBJECT src/mlib/alloc.c)
add_library(mongo::mlib::allocator ALIAS mongo-mlib-allocator)
# Use mongo::mlib's header search path to compile the owner. Linking the header-only
# target alone does not supply allocator storage.
target_link_libraries(mongo-mlib-allocator PRIVATE mongo::mlib)
target_compile_options(mongo-mlib-allocator PRIVATE ${mongoc-warning-options})
# Compile position-independent code so the object can be embedded in a shared
# library. Hide symbols by default: the accessor's source-level export annotations
# provide the external entry point where required.
set_target_properties(mongo-mlib-allocator PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    C_VISIBILITY_PRESET hidden
    )

if(BUILD_TESTING)
    # Build each linkage arrangement as a separate executable. The C translation
    # units compare allocator-state addresses and observe configuration changes
    # across translation units. The suffix distinguishes the executable and CTest
    # names, while `owner` selects where allocator storage comes from.
    function(_mongo_add_allocator_test suffix owner)
        set(test_target "mlib-alloc${suffix}-test")
        add_executable(${test_target}
            tests/test-mlib-alloc.c
            tests/test-mlib-alloc-peer.c
            )
        # For BSON-owned cases, obtain the accessor from BSON rather than linking
        # the allocator OBJECT target again.
        target_link_libraries(${test_target} PRIVATE mongo::mlib ${owner})
        # These warning options were selected for the C compiler. Restrict them to
        # C because those warnings might not be applicable to C++ compilation.
        target_compile_options(${test_target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:${mongoc-warning-options}>")
        set_target_properties(${test_target} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        # When a C++ compiler is available, add a C++11 translation unit and enable
        # the C harness's cross-language checks of the same allocator state.
        if(CMAKE_CXX_COMPILER)
            target_sources(${test_target} PRIVATE tests/test-mlib-alloc.cpp)
            target_compile_features(${test_target} PRIVATE cxx_std_11)
            target_compile_definitions(${test_target} PRIVATE MLIB_TEST_CXX)
        endif()
        add_test(NAME "mongoc/mlib/alloc${suffix}" COMMAND ${test_target})
    endfunction()

    # First test an executable that owns the allocator object directly.
    _mongo_add_allocator_test("" mongo::mlib::allocator)
    # Then test the enabled in-tree BSON owners: the static case exercises archive
    # extraction, and the shared case exercises the exported accessor. Skip these
    # cases for system libbson, whose allocator-owner implementation is outside
    # this build's control.
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

if(MONGO_CAN_VERIFY_HEADERS AND BUILD_TESTING)
    mongo_verify_headers(
        mlib
        USE_LIBRARIES mongo::mlib
        HEADERS src/mlib/*.h
    )
endif()
