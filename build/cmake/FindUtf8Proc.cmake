if(USE_BUNDLED_UTF8PROC STREQUAL "TRUE")
  add_subdirectory("../utf8proc-2.8.0" ".../cmake-build")
elseif (USE_BUNDLED_UTF8PROC STREQUAL "FALSE")
  find_package(PkgConfig)
  pkg_check_modules(PC_UTF8PROC REQUIRED libutf8proc)
  add_library(utf8proc STATIC IMPORTED)
  message("${PC_UTF8PROC_LINK_LIBRARIES} HERE")
  set_target_properties(utf8proc PROPERTIES COMPILE_OPTIONS ${PC_UTF8PROC_STATIC_CFLAGS} IMPORTED_LOCATION ${PC_UTF8PROC_LINK_LIBRARIES})
else ()
   message (FATAL_ERROR
            "USE_BUNDLED_UTF8PROC must be set to either TRUE or FALSE"
            )
endif()