#[===[

    Create a simple CTest fixture from a setup/cleanup command pair.

    add_test_fixture(
        <fixture_name>
        [REQUIRES ...]
        [WORKING_DIRECTORY <dir>]
        [ENVIRONMENT ...]
        [SETUP
            COMMAND [...]
            [TIMEOUT <N>]
            [REQUIRES ...]
            [WORKING_DIRECTORY <dir>]
            [ENVIRONMENT ...]]
        [CLEANUP
            COMMAND [...]
            [TIMEOUT <N>]
            [REQUIRES ...]
            [WORKING_DIRECTORY <dir>]
            [ENVIRONMENT ...]]
        )

    Refer: https://cmake.org/cmake/help/latest/prop_test/FIXTURES_REQUIRED.html

]===]
function (add_test_fixture name)
    # Pick out the setup/cleanup arguments
    cmake_parse_arguments (PARSE_ARGV 1 ARG "" "" "SETUP;CLEANUP")
    # Pick out the common arguments
    cmake_parse_arguments (__fxt "" "WORKING_DIRECTORY" "ENVIRONMENT;REQUIRES" ${ARG_UNPARSED_ARGUMENTS})
    # Error if anything else remains:
    if (__fxt_UNPARSED_ARGUMENTS)
        message (SEND_ERROR "Unhandled arguments for add_test_fixture: ${__fxt_UNPARSED_ARGUMENTS}")
        return ()
    endif ()
    # __fxt variables are intended to be used by callees
    set (__fxt_name "${name}")
    if (ARG_SETUP)
        _fxt_add_kind (setup SETUP ${ARG_SETUP})
    endif ()
    if (ARG_CLEANUP)
        _fxt_add_kind (cleanup CLEANUP ${ARG_CLEANUP})
    endif ()
endfunction ()

# Define a setup/cleanup for a fixture. Used by add_test_fixture
function (_fxt_add_kind kind KIND)
    cmake_parse_arguments (PARSE_ARGV 2 ARG "" "WORKING_DIRECTORY;TIMEOUT" "COMMAND;ENVIRONMENT;REQUIRES")
    if (ARG_UNPARSED_ARGUMENTS)
        message (SEND_ERROR "Unhandled arguments for ${KIND} in add_test_fixture: ${ARG_UNPARSED_ARGUMENTS}")
    endif ()

    # The "__fxt_*" variables come from the add_test_fixture() caller
    set (test "${__fxt_name}:${kind}")
    add_test (NAME "${test}" COMMAND ${ARG_COMMAND})

    # Set environment variables
    set_property (
        TEST "${test}"
        APPEND PROPERTY ENVIRONMENT
        ${__fxt_ENVIRONMENT}    # Common
        ${ARG_ENVIRONMENT}      # For this kind
        )

    # Set working directory
    if (__fxt_WORKING_DIRECTORY)
        # Common working directory
        set_property(TEST "${test}" PROPERTY WORKING_DIRECTORY "${__fxt_WORKING_DIRECTORY}")
    endif ()
    if (ARG_WORKING_DIRECTORY)
        # This-kind working directory
        set_property(TEST "${test}" PROPERTY WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}")
    endif ()

    # Timeout for this kind
    if (ARG_TIMEOUT)
        set_property(TEST "${test}" PROPERTY TIMEOUT "${ARG_TIMEOUT}")
    endif ()

    # Assign us as a fixture
    add_fixture_dependencies (
        "${test}"
        REQUIRES ${__fxt_REQUIRES} ${ARG_REQUIRES}
        "${KIND}" "${__fxt_name}"  # "KIND" becomes "SETUP" or "CLEANUP"
        )
endfunction ()

#[=[

    Define fixtures, and the dependencies between fixtures and tests

        add_fixture_dependencies(
            [<tests> ...]
            [REQUIRES <fixture-name> ...]
            [SETUP <fixture-name> ...]
            [CLEANUP <fixture-name> ...]
            )

    For each fixture in REQUIRES, each of <tests> will be set to depend on those
    fixtures.

    For each fixture in SETUP, each of <tests> will be marked as a setup-test.

    For each fixture in CLEANUP, each of <tests> will be marked as a
    cleanup-test.

]=]
function (add_fixture_dependencies)
    cmake_parse_arguments (PARSE_ARGV 0 ARG "" "" "REQUIRES;SETUP;CLEANUP")
    # Treat the unparsed arguments as the names of tests
    set_property (TEST ${ARG_UNPARSED_ARGUMENTS} APPEND PROPERTY FIXTURES_REQUIRED ${ARG_REQUIRES})
    set_property (TEST ${ARG_UNPARSED_ARGUMENTS} APPEND PROPERTY FIXTURES_SETUP ${ARG_SETUP})
    set_property (TEST ${ARG_UNPARSED_ARGUMENTS} APPEND PROPERTY FIXTURES_CLEANUP ${ARG_CLEANUP})
endfunction ()
