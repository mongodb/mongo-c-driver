set (MLIB_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}")

add_library (_mongo-mlib INTERFACE)
get_filename_component (_inc_dir "${MLIB_SRC_DIR}" DIRECTORY)
target_include_directories (_mongo-mlib INTERFACE "${_inc_dir}")
target_compile_definitions (_mongo-mlib INTERFACE MLIB_USER)
add_library (mongo::mlib ALIAS _mongo-mlib)

set (_components error path str thread user-check)
set (MLIB_SOURCES)
foreach (comp IN LISTS _components)
    set (test_file "${MLIB_SRC_DIR}/${comp}.test.c")
    if (EXISTS "${test_file}")
        add_executable (mlib.test.${comp} "${test_file}")
        target_link_libraries (mlib.test.${comp} PRIVATE mongo::mlib)
        add_test (mlib.test.${comp} mlib.test.${comp})
        list (APPEND MLIB_SOURCE "${test_file}")
    endif ()
    list (APPEND MLIB_SOURCES "${MLIB_SRC_DIR}/${comp}.h")
endforeach ()

# This is only here to support export() of mongo::mlib-linked targets.
# Nothing is actually installed
install (TARGETS _mongo-mlib EXPORT _mongo-mlib)
export (EXPORT _mongo-mlib)

function (target_use_mlib)
    foreach (t IN LISTS ARGV)
        target_link_libraries("${t}" PRIVATE $<BUILD_INTERFACE:mongo::mlib>)
    endforeach ()
endfunction ()
