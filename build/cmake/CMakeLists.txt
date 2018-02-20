add_subdirectory (make_dist)

set (build_cmake_MODULES
   FindResSearch.cmake
   FindSASL2.cmake
   FindSnappy.cmake
   LoadVersion.cmake
   MaintainerFlags.cmake
   MongoCPackage.cmake
)

set (build_cmake_PKGCONFIG
   libmongoc-1.0-config-version.cmake.in
   libmongoc-1.0-config.cmake.in
   libmongoc-static-1.0-config-version.cmake.in
   libmongoc-static-1.0-config.cmake.in
)

set_local_dist (build_cmake_DIST_local
   CMakeLists.txt
   ${build_cmake_MODULES}
   ${build_cmake_PKGCONFIG}
)

set (build_cmake_DIST
   ${build_cmake_DIST_local}
   ${build_cmake_make_dist_DIST}
   PARENT_SCOPE
)
