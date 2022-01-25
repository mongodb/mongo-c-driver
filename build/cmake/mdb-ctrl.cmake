#[[

This CMake script is intended to be executed with '-P' to control a MongoDB
server executable as a test fixture. It uses the following arguments:

DO={START,STOP} (required)
    The action to perform. Must be 'START' or 'STOP'

RUNDIR=<dirpath> (required)
    A base directory in which to base all server executions.

FIXTURE_NAME=<string> (required)
    A unique name of the fixture server to control. The name is arbitrary, but
    should be unique.

MDB_EXE=<filepath> (required)
    The path to a MongoDB server executable to use to control the test fixture.

MDB_PORT=<int> (required for DO=START)
    An integer TCP port on which the server should bind.

SERVER_ARGS=<semicolon-separated-list> (optional for DO=START)
    Additional arguments that should be given to the server executable when it
    is started. MAY NOT be any of `--port`, `--pidfilepath`, `--fork`,
    `--verbose`, `--logpath`, `--dbpath`, or `--shutdown`, as those options are
    controlled by this script.

---

If DO=START, and this script detects that there was a prior server test fixture
with the given name, it will ensure that the test fixture is not running before
trying to start it again.

If DO=STOP, this script will attempt to stop the server test fixture. If it
fails to stop, then this script will exit with an error

]]

# Fixture properties:
get_filename_component (__fixture_dir "${RUNDIR}/_test-db/${FIXTURE_NAME}" ABSOLUTE)
set (__mongod_exe "${MDB_EXE}")

# Start up a clean MongoDB server in the fixture directory. Existing data will be deleted.
function (_start port server_argv)
    if (IS_DIRECTORY "${__fixture_dir}"
            AND NOT EXISTS "${__fixture_dir}/stopped.stamp")
        _try_stop (_)
    endif ()
    file (REMOVE_RECURSE "${__fixture_dir}")
    file (MAKE_DIRECTORY "${__fixture_dir}/data")
    execute_process (
        COMMAND "${__mongod_exe}"
            ${server_argv}
            --port "${port}"
            --pidfilepath "${__fixture_dir}/mdb.pid"
            --fork
            --verbose
            --logpath "${__fixture_dir}/server.log"
            --dbpath "${__fixture_dir}/data"
        RESULT_VARIABLE retc
        )
    if (retc)
        message (SEND_ERROR "Failed to start the MongoDB server [${retc}]")
        _stop ()
    endif ()
    file (REMOVE "${__fixture_dir}/stopped.stamp")
endfunction ()

# Try to stop the fixture server, but not a hard-error if it fails
function (_try_stop okvar)
    execute_process(
        COMMAND "${__mongod_exe}" ${server_argv}
            --pidfilepath "${__fixture_dir}/mdb.pid"
            --dbpath "${__fixture_dir}/data"
            --shutdown
        RESULT_VARIABLE retc
        )
    if (retc)
        message (STATUS "Failed to stop the MongoDB server [${retc}]")
        set ("${okvar}" FALSE PARENT_SCOPE)
    else ()
        set ("${okvar}" TRUE PARENT_SCOPE)
        file (WRITE "${__fixture_dir}/stopped.stamp")
    endif ()
endfunction ()

# Stop the fixture server, or fail the test
function (_stop)
    _try_stop (did_stop)
    if (NOT did_stop)
        message (FATAL_ERROR "Failed to stop the MongoDB server")
    endif ()
endfunction ()

# Pick the action
if (DO STREQUAL "START")
    _start ("${MDB_PORT}" "${SERVER_ARGS}")
elseif (DO STREQUAL "STOP")
    _stop ()
else ()
    message (FATAL_ERROR "mdb-ctrl.cmake requires a DO action of 'START' or 'STOP'")
endif ()
