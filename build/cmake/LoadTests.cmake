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
    COMMAND "${TEST_LIBMONGOC_EXE}" --list-tests --include-meta --no-fork
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
    add_test ("${name}" "${TEST_LIBMONGOC_EXE}" --ctest-run "${ctest_run}" ${ARGN})
    set_tests_properties ("${name}" PROPERTIES
        # test-libmongoc expects to execute in the root of the source directory
        WORKING_DIRECTORY "${SRC_ROOT}"
        # If a test emits '@@ctest-skipped@@', this tells us that the test is
        # skipped.
        SKIP_REGULAR_EXPRESSION "@@ctest-skipped@@"
        )
endfunction ()


function (_define_test name)
    # Parse the "test arguments" that come from the `meta` field of the tests
    cmake_parse_arguments(
        PARSE_ARGV 1 ARG
        "USES_LIVE_SERVER"
        "TIMEOUT;RUN_NAME;MIN_SERVER_VERSION;MAX_SERVER_VERSION;USE_SERVER"
        "LABELS;USES")
    # Default timeout
    if (NOT ARG_TIMEOUT)
        set (ARG_TIMEOUT 10)
    endif ()
    # Default RUN_NAME (The name passed to --ctest-run)
    if (NOT ARG_RUN_NAME)
        set (ARG_RUN_NAME "${name}")
    endif ()
    # Generate messages for unrecognized arguments
    if (ARG_UNPARSED_ARGUMENTS)
        message ("-- NOTE: Test '${name}' gave unrecognized metadata: ${ARG_UNPARSED_ARGUMENTS}")
    endif ()

    # If this test uses a live server generate a version of the test that runs
    # against each of the default server fixtures.
    if (ARG_USES_LIVE_SERVER)
        set (args "${ARGN}")
        list (REMOVE_ITEM args "USES_LIVE_SERVER")
        # _MDB_DEFAULT_TEST_FIXTURES comes from MongoDB-CTestData
        foreach (fxt IN LISTS _MDB_DEFAULT_TEST_FIXTURES)
            _define_test (
                "${name}@${fxt}"
                 RUN_NAME "${name}"
                 LABELS ${ARG_LABELS}
                        "uses-live-server"
                        "server-version=${_MDB_FIXTURE_${fxt}_SERVER_VERSION}"
                 USE_SERVER "${fxt}"
                 MIN_SERVER_VERSION "${ARG_MIN_SERVER_VERSION}"
                 MAX_SERVER_VERSION "${ARG_MAX_SERVER_VERSION}"
                 USES ${USES}
                 )
        endforeach ()
        if (NOT _MDB_DEFAULT_TEST_FIXTURES)
            add_test ("${name}@no-fixtures" nil)
            set_tests_properties (
                "${name}@no-fixtures" PROPERTIES
                DISABLED TRUE
                LABELS "uses-live-server;${ARG_LABELS}"
                )
        endif ()
        return ()
    endif ()

    set (fixtures ${ARG_USES})
    if (ARG_USE_SERVER)
        set (fxt "${ARG_USE_SERVER}")
        list (APPEND fixtures "${fxt}")
        set (server_version "${_MDB_FIXTURE_${fxt}_SERVER_VERSION}")
        if (ARG_MIN_SERVER_VERSION AND server_version VERSION_LESS ARG_MIN_SERVER_VERSION)
            return ()
        elseif (ARG_MAX_SERVER_VERSION AND server_version VERSION_GREATER ARG_MAX_SERVER_VERSION)
            return ()
        endif ()
    endif ()

    _register_test ("${name}" "${ARG_RUN_NAME}")
    set_tests_properties ("${name}" PROPERTIES
        FIXTURES_REQUIRED "${fixtures}"
        RESOURCE_LOCK "${fixtures}"
        ENVIRONMENT "${all_env};MONGOC_TEST_URI=mongodb://localhost:${_MDB_FIXTURE_${fxt}_PORT}"
        LABELS "${ARG_LABELS}"
        )
endfunction ()

if (NOT _MDB_DEFAULT_TEST_FIXTURES)
    message ("-- Note: No default test fixtures were defined, so tests requiring a ")
    message ("         live server will be skipped/disabled.")
endif ()

# Generate the test definitions
message ("-- Loading tests (this may take a moment if there are many server fixtures)")
foreach (line IN LISTS lines)
    if (NOT line MATCHES "^/")
        # Only generate if the line begins with `/`, which all tests should.
        continue ()
    endif ()
    separate_arguments (listing UNIX_COMMAND "${line}")
    _define_test (${listing})
endforeach ()

