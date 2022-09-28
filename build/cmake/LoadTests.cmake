# This file is include()'d by CTest. It executes test-libmongoc to get a list
# of all tests that are registered. Each test is then defined as a CTest test,
# allowing CTest to control the execution, parallelization, and collection of
# test results.

if (NOT EXISTS "${TEST_LIBMONGOC_EXE}")
    # This will fail if 'test-libmongoc' is not compiled yet.
    message (WARNING "The test executable ${TEST_LIBMONGOC_EXE} is not present. "
                     "Its tests will not be registered")
    add_test (mongoc/not-found NOT_FOUND)
    return ()
endif ()

# Get the list of tests
execute_process (
    COMMAND "${TEST_LIBMONGOC_EXE}" --list-tests --no-fork
    OUTPUT_VARIABLE tests_out
    WORKING_DIRECTORY "${SRC_ROOT}"
    RESULT_VARIABLE retc
    )
if (retc)
    # Failed to list the tests. That's bad.
    message (FATAL_ERROR "Failed to run test-libmongoc to discover tests [${retc}]:\n${tests_out}")
endif ()

# Split lines on newlines
string (REPLACE "\n" ";" lines "${tests_out}")

# XXX: Allow individual test cases to specify the fixtures they want.
set (all_fixtures "mongoc/fixtures/fake_imds")
set (all_env
    MCD_TEST_AZURE_IMDS_HOST=localhost:14987  # Refer: Fixtures.cmake
    )

function (_register_test name ctest_run)
    # Define the test. Use `--ctest-run` to tell it that CTest is in control.
    add_test ("${name}" "${TEST_LIBMONGOC_EXE}" --ctest-run "${ctest_run}" ${ARGN})
    set_tests_properties ("${name}" PROPERTIES
        # test-libmongoc expects to execute in the root of the source directory
        WORKING_DIRECTORY "${SRC_ROOT}"
        # If a test emits '@@ctest-skipped@@', this tells us that the test is
        # skipped.
        SKIP_REGULAR_EXPRESSION "@@ctest-skipped@@"
        # 45 seconds of timeout on each test.
        TIMEOUT 45
        )
endfunction ()

# Generate the test definitions
foreach (line IN LISTS lines)
    if (NOT line MATCHES "^/")
        # Only generate if the line begins with `/`, which all tests should.
        continue ()
    endif ()
    # The new test name is prefixed with 'mongoc'
    separate_arguments (listing UNIX_COMMAND "${line}")
    list (GET listing 0 test_name)
    set (meta "${listing}")
    list (REMOVE_AT meta 0)
    set (test "mongoc${test_name}")
    if (NOT _MDB_DEFAULT_TEST_FIXTURES OR NOT (meta MATCHES "uses-live-server"))
        _register_test ("${test}" "${test_name}")
        set_tests_properties ("${test}" PROPERTIES
            TIMEOUT 15
            LABELS "${meta}"
            ENVIRONMENT "${all_env}"
            FIXTURES_REQUIRED "${all_fixtures}"
            )
    else ()
        foreach (fxt IN LISTS _MDB_DEFAULT_TEST_FIXTURES)
            set (qualname "${fxt}/${test}")
            _register_test ("${qualname}" "${test_name}")
            set_tests_properties ("${qualname}" PROPERTIES
                FIXTURES_REQUIRED "${fxt};${_MDB_TRANSITIVE_FIXTURES_OF_${fxt}}"
                RESOURCE_LOCK "${fxt}"
                TIMEOUT 15
                LABELS "${meta}"
                ENVIRONMENT "${all_env};MONGOC_TEST_URI=mongodb://localhost:${_MDB_FIXTURE_${fxt}_PORT}"
                FIXTURES_REQUIRED "${all_fixtures}"
                )
        endforeach ()
    endif ()
endforeach ()
