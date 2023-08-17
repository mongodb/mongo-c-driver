include(CMakeFindDependencyMacro)
find_dependency(Threads)  # Required for Threads::Threads

# Import common targets first:
include("${CMAKE_CURRENT_LIST_DIR}/bson-targets.cmake")

# Now import the targets of each link-kind that's available. Only the targets of
# libbson libraries that were actually installed alongside this file will be
# imported.
foreach(set IN ITEMS bson_shared bson_static)
    set(inc "${CMAKE_CURRENT_LIST_DIR}/${set}-targets.cmake")
    if(EXISTS "${inc}")
        include("${inc}")
    endif()
endforeach()
