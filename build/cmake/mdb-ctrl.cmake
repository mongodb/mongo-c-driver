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
