# This file is include()'d by CTest. It executes test-libmongoc to get a list
# of all tests that are registered. Each test is then defined as a CTest test,
# allowing CTest to control the execution, parallelization, and collection of
# test results.

if(NOT EXISTS "${TEST_LIBMONGOC_EXE}")
    # This will fail if 'test-libmongoc' is not compiled yet.
    message(WARNING "The test executable ${TEST_LIBMONGOC_EXE} is not present. "
                     "Its tests will not be registered")
    add_test(mongoc/not-found NOT_FOUND)
    return()
endif()

# Get the list of tests. This command emits CMake code that defines variables for
# all test cases defined in the suite
execute_process(
    COMMAND "${TEST_LIBMONGOC_EXE}" --tests-cmake --no-fork
    OUTPUT_VARIABLE tests_cmake
    WORKING_DIRECTORY "${SRC_ROOT}"
    RESULT_VARIABLE retc
    )
if(retc)
    # Failed to list the tests. That's bad.
    message(FATAL_ERROR "Failed to run test-libmongoc to discover tests [${retc}]:\n${tests_out}")
endif()

# Execute the code that defines the test case information
cmake_language(EVAL CODE "${tests_cmake}")

# Define environment variables that are common to all test cases
set(all_env
    TEST_KMS_PROVIDER_HOST=localhost:14987  # Refer: Fixtures.cmake
    )

# The emitted code defines a list MONGOC_TESTS with the name of every test case
# in the suite.
foreach(casename IN LISTS MONGOC_TESTS)
    set(name "mongoc${casename}")
    # Run the program with --ctest-run to select only this one test case
    add_test("${name}" "${TEST_LIBMONGOC_EXE}" --ctest-run "${casename}")
    # The emitted code defines a TAGS list for every test case that it emits. We use
    # these as the LABELS for the test case
    set(labels "${MONGOC_TEST_${casename}_TAGS}")

    # Find what test fixtures the test wants by inspecting labels. Each "uses:"
    # label defines the names of the test fixtures that a particular case requires
    set(fixtures "${labels}")
    list(FILTER fixtures INCLUDE REGEX "^uses:")
    list(TRANSFORM fixtures REPLACE "^uses:(.*)$" "mongoc/fixtures/\\1")

    # Add a label for all test cases generated via this script so that they
    # can be (de)selected separately:
    list(APPEND labels test-libmongoc-generated)
    # Set up the test:
    set_tests_properties("${name}" PROPERTIES
        # test-libmongoc expects to execute in the root of the source directory
        WORKING_DIRECTORY "${SRC_ROOT}"
        # If a test emits '@@ctest-skipped@@', this tells us that the test is
        # skipped.
        SKIP_REGULAR_EXPRESSION "@@ctest-skipped@@"
        # 45 seconds of timeout on each test.
        TIMEOUT 45
        # Common environment variables:
        ENVIRONMENT "${all_env}"
        # Apply the labels
        LABELS "${labels}"
        # Fixture requirements:
        FIXTURES_REQUIRED "${fixtures}"
    )
endforeach()
