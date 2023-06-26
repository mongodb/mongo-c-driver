include(CMakeFindDependencyMacro)
find_dependency(Threads)  # Required for Threads::Threads

include("${CMAKE_CURRENT_LIST_DIR}/bson-targets.cmake")
