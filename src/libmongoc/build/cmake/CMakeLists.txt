set_local_dist (src_libmongoc_build_cmake_DIST_local
   CMakeLists.txt
)

set (src_libmongoc_build_cmake_DIST
   ${src_libmongoc_build_cmake_DIST_local}
   PARENT_SCOPE
)
