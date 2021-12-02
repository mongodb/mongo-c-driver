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

function (_register_test name ctest_run)
    # Define the test. Use `--ctest-run` to tell it that CTest is in control.
    add_test ("${name}" "${TEST_LIBMONGOC_EXE}" --ctest-run "${line}" ${ARGN})
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
    set (test "mongoc${line}")
    if (NOT _MDB_TEST_FIXTURES_AUTO_USE)
        _register_test ("${test}" "${line}")
    else ()
        foreach (fxt IN LISTS _MDB_TEST_FIXTURES_AUTO_USE)
            set (qualname "${fxt}/${test}")
            _register_test ("${qualname}" "${line}")
            set_tests_properties ("${qualname}" PROPERTIES
                FIXTURES_REQUIRED "${fxt}"
                RESOURCE_LOCK "${fxt}"
                ENVIRONMENT "MONGOC_TEST_URI=mongodb://localhost:${_MDB_FIXTURE_${fxt}_PORT}"
                )
        endforeach ()
    endif ()
endforeach ()
