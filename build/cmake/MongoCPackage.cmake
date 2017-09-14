set (PACKAGE_INCLUDE_INSTALL_DIRS ${MONGOC_HEADER_INSTALL_DIR})
set (PACKAGE_LIBRARY_INSTALL_DIRS lib)
set (PACKAGE_LIBRARIES mongoc-1.0)

include (CMakePackageConfigHelpers)

foreach (prefix "1.0" "static-1.0")
   foreach (suffix "config.cmake" "config-version.cmake")
      configure_package_config_file (
         build/cmake/libmongoc-${prefix}-${suffix}.in
         ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         INSTALL_DESTINATION lib/cmake/libmongoc-${prefix}
         PATH_VARS PACKAGE_INCLUDE_INSTALL_DIRS PACKAGE_LIBRARY_INSTALL_DIRS
      )

      install (
         FILES
           ${CMAKE_CURRENT_BINARY_DIR}/libmongoc-${prefix}-${suffix}
         DESTINATION
           lib/cmake/libmongoc-${prefix}
      )
   endforeach ()
endforeach ()
