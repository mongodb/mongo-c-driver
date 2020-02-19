set (src_libbson_build_cmake_MODULES
   CheckAtomics.cmake
)

set_local_dist (src_libbson_build_cmake_DIST_local
   CMakeLists.txt
   ${src_libbson_build_cmake_MODULES}
)

set (src_libbson_build_cmake_DIST
   ${src_libbson_build_cmake_DIST_local}
   PARENT_SCOPE
)
