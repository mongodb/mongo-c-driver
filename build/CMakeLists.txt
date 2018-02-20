add_subdirectory (cmake)

set_local_dist (build_DIST_local
   CMakeLists.txt
   maintainer-flags.txt
)

set (build_DIST
   ${build_DIST_local}
   ${build_cmake_DIST}
   PARENT_SCOPE
)
