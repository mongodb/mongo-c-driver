#[[
    This file defines CTest test fixtures used by our tests.
]]
include (QuickFixtures)

# We use Python to execute the proc_ctl script
find_package (Python3 COMPONENTS Interpreter)

if (NOT TARGET Python3::Interpreter)
    message (WARNING "Python3 was not found, so test fixtures cannot be defined")
    return ()
endif ()

get_filename_component(_MONGOC_BUILD_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)

# The command prefix to execute proc_ctl.py
set (_MONGOC_PROC_CTL_COMMAND "$<TARGET_FILE:Python3::Interpreter>" -u -- "${_MONGOC_BUILD_SCRIPT_DIR}/proc_ctl.py")

#[=[

    Define a simple test fixture that spawns a running process and stops it for
    the cleanup.

    mongo_define_subprocess_fixture(
        <name>
        COMMAND [...]
        [SPAWN_WAIT <n>]
        [STOP_WAIT <n>]
        [WORKING_DIRECTORY <dir>]
        )

    Creates a test fixture with name given by <name>. For setup, the
    long-running process given by COMMAND will be spawned (using proc_ctl.py).
    During cleanup, the process will be stopped by sending it an interupt
    signal.

    SPAWN_WAIT specifies a number of seconds that should be waited to ensure the
    process continues running. The default is one second. NOTE: A successful
    setup phase will ALWAYS wait this long, even if the process is stable within
    a shorter period of time. This should only be used to catch processes that
    may start up but could fail within a few seconds.

    STOP_WAIT specifies a number of seconds that should be waited to allow the
    process to stop after we send it a stopping signal. The default is five
    seconds. Unlike with SPAWN_WAIT, the cleanup phase will only run for as long
    as it takes the process to stop.

    WORKING_DIRECTORY specifies an alternate working directory for the spawned
    subprocess.

]=]
function (mongo_define_subprocess_fixture name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "SPAWN_WAIT;STOP_WAIT;WORKING_DIRECTORY" "COMMAND")
    # Default spawn-wait time is one second
    if (NOT ARG_SPAWN_WAIT)
        set (ARG_SPAWN_WAIT 1)
    endif ()
    # Default stop-wait time is five seconds
    if (NOT ARG_STOP_WAIT)
        set (ARG_STOP_WAIT 5)
    endif ()
    # Default working directory is the current binary directory
    if (NOT ARG_WORKING_DIRECTORY)
        set (ARG_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endif ()
    # Require a command:
    if (NOT ARG_COMMAND)
        message (SEND_ERROR "mongo_define_subprocess_fixture(${name}) requires a COMMAND")
        return ()
    endif ()
    # Other arguments are an error:
    if (ARG_UNPARSED_ARGUMENTS)
        message (SEND_ERROR "Unknown arguments given to mongo_define_subprocess_fixture(): ${ARG_UNPARSED_ARGUMENTS}")
    endif ()
    # Create a control directory based on the fixture name
    string (MAKE_C_IDENTIFIER ident "${name}")
    get_filename_component (ctl_dir "${CMAKE_CURRENT_BINARY_DIR}/${ident}.ctl" ABSOLUTE)
    # Define the fixture around proc_ctl:
    add_test_fixture ("${name}"
        SETUP COMMAND
            ${_MONGOC_PROC_CTL_COMMAND} start
                --ctl-dir=${ctl_dir}
                --cwd=${ARG_WORKING_DIRECTORY}
                --spawn-wait=${ARG_SPAWN_WAIT}
                -- ${ARG_COMMAND}
        CLEANUP COMMAND
            ${_MONGOC_PROC_CTL_COMMAND} stop
                --ctl-dir=${ctl_dir}
                --if-not-running=ignore
        )
endfunction ()

# This variable will later be used to inject the host of the fake IMDS server
# into the appropriate test case.
set (_MONGOC_FAKE_IMDS_HOST "localhost:14987")

# Create a fixture that runs a fake Azure IMDS server
mongo_define_subprocess_fixture(
    mongoc/fixtures/fake_imds
    SPAWN_WAIT 0.2
    COMMAND
        "$<TARGET_FILE:Python3::Interpreter>" -u --
        "${_MONGOC_BUILD_SCRIPT_DIR}/bottle.py" fake_azure:imds
            --bind localhost:14987  # Port 14987 chosen arbitrarily
    )
