#[[

Find MongoDB executables on the system, and define utilities for using them.

This attempts to find as many MongoDB executables as possible. It scans ENV{PATH},
as well as ENV{M_PREFIX} (if you have 'm' installed, all installed versions will
be found by this module.)

This module defines a global property `MONGODB_FOUND_VERSIONS`, which is a list
of every unique MongoDB version that it found. For each version 'XYZ' that it
found, it will define an additional global property 'MONGODB_${XYZ}_PATH' that
contains the absolute path to the mongod server executable for that version.

]]

if (MongoDB_FIND_COMPONENTS)
    message (SEND_ERROR "FindMongoDB does not take any components")
endif ()

include (FindPackageHandleStandardArgs)
include (CMakeParseArguments)

define_property (GLOBAL PROPERTY MONGODB_FOUND_VERSIONS
    BRIEF_DOCS "Versions of MongoDB that have been found"
    FULL_DOCS "List of versions of MongoDB server executables that have been found"
    )

set(MONGODB_DEFAULT_TEST_FIXTURE_VERSIONS
    "ALL" CACHE STRING
    "List of MongoDB server versions against which to generate default test fixtures"
    )

#[[
    Attempt to discern the MongoDB version of the given MongoDB server
    executable, and save that information in a global property for later use.
]]
function (_mdb_discover_exe mongod_exe)
    # Create an ident for the filepath, since cache variables need to be cleanly named
    string (MAKE_C_IDENTIFIER "${mongod_exe}" mongod_exe_id)
    # The outupt from the executable will be cached in this variable:
    set (_cache_var "_MONGOD_EXE_${mongod_exe_id}_VERSION_OUTPUT")
    # If we haven't run it, execute it now:
    if (NOT DEFINED "${_cache_var}")
        message (STATUS "Checking mongod executable: ${mongod_exe}")
        execute_process (
            COMMAND "${mongod_exe}" --version
            TIMEOUT 1
            OUTPUT_VARIABLE out
            RESULT_VARIABLE retc
            )
        if (retc)
            # Execution failed, for some reason. Probably not a valid executable?
            message (STATUS "Ignoring failing executable: ${mongod_exe}")
            set ("${_cache_var}" FAIL CACHE INTERNAL "Failure from ${mongod_exe}")
        else ()
            # Escape the output, since cache variables cannot have some characters
            string (REPLACE "$" "$dollar" out "${out}")
            string (REPLACE "\n" "$newline" out "${out}")
            # Define a cache variable so that we don't need to run the executable every time
            set ("${_cache_var}" "${out}"
                CACHE INTERNAL "Output from: ${mongod_exe} --version")
        endif ()
    endif ()
    # Get the output from --version for the given executable
    set (_version_output "${${_cache_var}}")
    if (_version_output STREQUAL "FAIL")
        # This executable didn't succeed when executed with --version
        return ()
    endif ()
    # The version output was escaped when stored in the cache
    string (REPLACE "$newline" "\n" _version_output "${_version_output}")
    string (REPLACE "$dollar" "$" _version_output "${_version_output}")
    if (_version_output MATCHES [["version": "([0-9]+\.[0-9]+\.[0-9])((-[0-9a-zA-Z]+)([^"]*))?",]])
        # Match newer versions' output
        set (version "${CMAKE_MATCH_1}${CMAKE_MATCH_3}")
    elseif (_version_output MATCHES [[db version v([0-9]+\.[0-9]+\.[0-9]+)]])
        # Match older versions' output
        set (version "${CMAKE_MATCH_1}")
    else ()
        # Didn't understand the output of this one
        message (WARNING "Unknown version output (from ${mongod_exe}): ${_version_output}")
        return ()
    endif ()
    # Define a property to store the path for this version
    set (_version_var "MONGODB_${version}_PATH")
    get_property (already GLOBAL PROPERTY "${_version_var}")
    if (already)
        # We've already found an executable for this version
        return ()
    endif ()
    # Define that property
    define_property (GLOBAL PROPERTY "${_version_var}"
        BRIEF_DOCS "Path for MongoDB ${version} Server"
        FULL_DOCS "Absolute path to the server executable for MongoDB version ${version}"
        )
    set_property (GLOBAL PROPERTY "${_version_var}" "${mongod_exe}")
    # Append to the global list of versions that we have found
    set_property (GLOBAL APPEND PROPERTY MONGODB_FOUND_VERSIONS "${version}")
endfunction ()

#[[ Scan in 'bindir' for any mongod executables with the given 'extension' file extension ]]
macro (_scan_for_mdb bindir extension)
    set (__bindir "${bindir}")
    set (__ext "${extension}")
    get_filename_component (candidate "${__bindir}/mongod${__ext}" ABSOLUTE)
    if (EXISTS "${candidate}")
        _mdb_discover_exe ("${candidate}")
    endif ()
endmacro ()

#[[ Find them all ]]
function (_mongodb_find)
    # Check if the user has 'm' installed:
    if (DEFINED ENV{M_PREFIX})
        set (m_prefix "$ENV{M_PREFIX}")
    else ()
        # It may be in /usr/local, if not overriden
        set (m_prefix "/usr/local")
    endif ()

    # Glob each version that is installed by 'm'
    file (GLOB m_version_dirs "${m_prefix}/m/versions/*/bin")

    # Collect all paths from the environment
    set (env_path "$ENV{PATH}")
    if (NOT WIN32)
        string (REPLACE ":" ";" env_path "$ENV{PATH}")
    endif ()
    # Remove dups
    set (paths ${m_version_dirs} ${env_path})
    list (REMOVE_DUPLICATES paths)

    # Scan for executables
    foreach (bindir IN LISTS paths)
        if (DEFINED ENV{PATHEXT})
            # Windows defines PATHEXT as part of its executable search algorithm
            foreach (ext IN LISTS ENV{PATHEXT})
                _scan_for_mdb ("${bindir}" "${ext}")
            endforeach ()
        else ()
            # Other platforms do not use extensions, so use an empty string
            _scan_for_mdb ("${bindir}" "")
        endif ()
    endforeach ()

    # Print versions of MongoDB that we have found.
    get_property (found GLOBAL PROPERTY MONGODB_FOUND_VERSIONS)
    set (new_found "${found}")
    # Only print new ones, if we are running another time.
    list (REMOVE_ITEM new_found ~~~ ${_MDB_PREVIOUSLY_FOUND_VERSIONS})
    if (new_found)
        set_property (GLOBAL PROPERTY MONGODB_FOUND_VERSIONS "${new_found}")
        message (STATUS "The following MongoDB server executable versions were found:")
        foreach (version IN LISTS new_found)
            get_property (path GLOBAL PROPERTY "MONGODB_${version}_PATH")
            message (STATUS "  - ${version}:")
            message (STATUS "      ${path}")
        endforeach ()
    endif ()
    set (_MDB_PREVIOUSLY_FOUND_VERSIONS "${found}" CACHE INTERNAL "")
    if (found)
        set (MongoDB_FOUND TRUE PARENT_SCOPE)
    endif ()
endfunction ()

# Do the finding
_mongodb_find ()

#[[
    Generate CTest test fixtures for every MongoDB version that has been found, as
    determined by the MONGODB_FOUND_VERSIONS global property. The generated fixtures
    are named as 'default-<version>' for each '<version>' that was found.
]]
function (mongodb_setup_default_fixtures)
    set (versions ${MONGODB_DEFAULT_TEST_FIXTURE_VERSIONS})
    if (versions STREQUAL "ALL")
        get_cmake_property (versions MONGODB_FOUND_VERSIONS)
    endif ()
    foreach (ver IN LISTS versions)
        mongodb_create_fixture (
            "mdb/fixture/default/${ver}" VERSION "${ver}"
            SERVER_ARGS --setParameter enableTestCommands=1
            DEFAULT
            )
    endforeach ()
endfunction ()

#[[
    Create a CTest test fixture that will start, stop, and clean up a MongoDB server
    instance for other tests.

        mongodb_create_fixture(
            <name> VERSION <version>
            [DEFAULT]
            [PORT_VARIABLE <port-varname>]
            [SERVER_ARGS <args> ...]
        )

    <name> and <version> are the only required arguments. VERSION must specify a
    MongoDB server version that has a known path. There must be a
    MONGODB_<version>_PATH global property that defines the path to a mongod
    executable file. These global properties are set by the FindMongoDB.cmake
    module when it is first imported. The list of available versions can be
    found in the MONGODB_FOUND_VERSIONS global property.

    <port-varname> is an output variable name. Each generated fixture uses a different
    TCP port when listening so that fixtures can execute in parallel without contending.
    This variable will be set in the caller's scope to the integer TCP port that will
    be used by the test fixture.

    <args>... is a list of command-line arguments to supply to the server when it is
    started. This can be any option *except* '--fork', '--port', '--dbpath', '--logpath',
    or '--pidfilepath', all of which are already specified for the test fixture.

    The fixture <name> can then be associated with tests via the FIXTURES_REQUIRED
    test property.

    If [DEFAULT] is specified, then the fixture will be added to a default list
    that will be used to populate live-server tests that do not otherwise
    specify a server test fixture
]]
function (mongodb_create_fixture name)
    cmake_parse_arguments (ARG
        "DEFAULT"
        "PORT_VARIABLE;VERSION"
        "SERVER_ARGS"
        ${ARGN}
        )
    if (NOT ARG_VERSION)
        message (SEND_ERROR "A VERSION is required")
        return ()
    endif ()
    get_cmake_property (path "MONGODB_${ARG_VERSION}_PATH")
    if (NOT path)
        message (SEND_ERROR "Cannot create a test fixture for MongoDB version ${ARG_VERSION}, which is not found")
    endif ()
    get_cmake_property (port "_MDB_UNUSED_PORT")
    math (EXPR next "${port} + 3")
    set_property (GLOBAL PROPERTY _MDB_UNUSED_PORT "${next}")
    add_test (NAME ${name}/setup
        COMMAND "${CMAKE_COMMAND}"
                -D "FIXTURE_NAME=${name}"
                -D "RUNDIR=${PROJECT_BINARY_DIR}"
                -D "MDB_EXE=${path}"
                -D MDB_PORT=${port}
                -D DO=START
                -D "SERVER_ARGS=${ARG_SERVER_ARGS}"
                -P ${_MDB_SCRIPT_DIR}/mdb-ctrl.cmake
            )
    add_test (NAME ${name}/cleanup
        COMMAND "${CMAKE_COMMAND}"
                -D "FIXTURE_NAME=${name}"
                -D "RUNDIR=${PROJECT_BINARY_DIR}"
                -D "MDB_EXE=${path}"
                -D DO=STOP
                -P ${_MDB_SCRIPT_DIR}/mdb-ctrl.cmake
            )
    set_property (TEST ${name}/setup PROPERTY FIXTURES_SETUP "${name}")
    set_property (TEST ${name}/cleanup PROPERTY FIXTURES_CLEANUP "${name}")
    set_property (
        TEST ${name}/setup ${name}/cleanup
        PROPERTY TIMEOUT 10)
    if (ARG_PORT_VARIABLE)
        set ("${ARG_PORT_VARIABLE}" "${port}" PARENT_SCOPE)
    endif ()
    set_property (TARGET __mdb-meta
        APPEND PROPERTY _CTestData_CONTENT
        "# Test fixture '${name}'"
        "set(_fxt [[${name}]])"
        "set(\"_MDB_FIXTURE_\${_fxt}_PORT\" ${port})\n"
        )
    set_property(TARGET __mdb-meta APPEND PROPERTY _ALL_FIXTURES "${name}")
    if (ARG_DEFAULT)
        set_property (TARGET __mdb-meta APPEND PROPERTY _DEFAULT_FIXTURES "${name}")
    endif ()
endfunction ()

function (mongodb_create_replset_fixture name)
    cmake_parse_arguments (ARG
        "DEFAULT"
        "PORT_VARIABLE;VERSION;COUNT;REPLSET_NAME"
        "SERVER_ARGS"
        ${ARGN}
        )
    if (NOT ARG_COUNT)
        set (ARG_COUNT 3)
    endif ()
    set (children)
    set(members)
    set(_id 0)
    foreach (n RANGE 1 "${ARG_COUNT}")
        mongodb_create_fixture("${name}/rs${n}"
            VERSION "${ARG_VERSION}"
            SERVER_ARGS ${SERVER_ARGS}
                --replSet "${ARG_REPLSET_NAME}"
                --setParameter enableTestCommands=1
            PORT_VARIABLE final_port
            )
        list (APPEND children "${name}/rs${n}")
        list(APPEND members "{_id: ${_id}, host: 'localhost:${final_port}'}")
        math(EXPR _id "${_id}+1")
    endforeach()
    get_cmake_property(mdb_exe MONGODB_${ARG_VERSION}_PATH)
    get_filename_component(mdb_dir "${mdb_exe}/" DIRECTORY)
    unset(_msh_path CACHE)
    find_program(_msh_path
        NAMES mongosh mongo
        HINTS "${mdb_dir}"
        NAMES_PER_DIR)
    set(init_js "${CMAKE_CURRENT_BINARY_DIR}/_replset-${name}-init.js")
    string(REPLACE ";" ", " members "${members}")
    file(WRITE "${init_js}"
        "var mems = [${members}];
        rs.initiate({
            _id: '${ARG_REPLSET_NAME}',
            members: mems,
        });
        var i = 0;
        for (;;) {
            if (rs.config().members.length == mems.length)
                break;
            sleep(1);
            ++i;
            if (i == 10000) {
                assert(false, 'Members did not connect')
            }
        }
        ")
    add_test (
        NAME "${name}"
        COMMAND "${_msh_path}"
            --port ${final_port}
            --norc "${init_js}"
        )
    set_tests_properties ("${name}"
        PROPERTIES FIXTURES_REQUIRED "${children}"
        FIXTURES_SETUP "${name}"
        )
    set_property(TARGET __mdb-meta APPEND PROPERTY _ALL_FIXTURES "${name}")
    if (ARG_DEFAULT)
        set_property(TARGET __mdb-meta APPEND PROPERTY _DEFAULT_FIXTURES "${name}")
    endif ()
    set_property(TARGET __mdb-meta
        APPEND PROPERTY _CTestData_CONTENT
            "# replSet fixture '${name}'"
            "set(_fxt [[${name}]])"
            "set(\"_MDB_FIXTURE_\${_fxt}_PORT\" ${final_port})"
            "set(\"_MDB_TRANSITIVE_FIXTURES_OF_\${_fxt}\" [[${children}]])\n"
            )
endfunction ()

set (_MDB_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set_property (GLOBAL PROPERTY _MDB_UNUSED_PORT 21231)

# The __mdb-meta target is used only for attaching metadata to be used as part of generator expressions
if (NOT TARGET __mdb-meta)
    add_custom_target (__mdb-meta)
    set (lines
        [[# THIS FILE IS GENERATED. DO NOT EDIT.]]
        "set(_MDB_ALL_TEST_FIXTURES     [[$<TARGET_PROPERTY:__mdb-meta,_ALL_FIXTURES>]])"
        "set(_MDB_DEFAULT_TEST_FIXTURES [[$<TARGET_PROPERTY:__mdb-meta,_DEFAULT_FIXTURES>]])"
        ""
        "$<JOIN:$<TARGET_PROPERTY:__mdb-meta,_CTestData_CONTENT>,\n>\n"
        )
    string (REPLACE ";" "\n" content "${lines}")
    file (GENERATE
        OUTPUT "${PROJECT_BINARY_DIR}/MongoDB-CTestData.cmake"
        CONTENT "${content}")
    set_target_properties (__mdb-meta PROPERTIES
        _ALL_FIXTURES ""
        _DEFAULT_FIXTURES ""
        _CTestData_CONTENT ""
        )
    set_property (DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${PROJECT_BINARY_DIR}/MongoDB-CTestData.cmake")
endif ()

get_cmake_property(versions MONGODB_FOUND_VERSIONS)
string(REPLACE ";" ", " versions "${versions}")
message(STATUS "MongoDB versions ${versions} are available for test fixtures")
