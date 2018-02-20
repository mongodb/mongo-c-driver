file (GLOB_RECURSE examples_DIST_cs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c)
file (GLOB_RECURSE examples_DIST_shs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.sh)

set_dist_list (examples_DIST
   CMakeLists.txt
   cmake/find_package/CMakeLists.txt
   cmake/find_package_static/CMakeLists.txt
   ${examples_DIST_cs}
   ${examples_DIST_shs}
)
