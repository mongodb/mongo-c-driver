if(USE_BUNDLED_UTF8PROC STREQUAL "TRUE")
  message (STATUS "Enabling utf8proc (bundled)")
  add_library (utf8proc_obj OBJECT "${UTF8PROC_SOURCES}")
  set_property (TARGET utf8proc_obj PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  set (SOURCES ${SOURCES} $<TARGET_OBJECTS:utf8proc_obj>)
  target_compile_definitions (utf8proc_obj PUBLIC UTF8PROC_STATIC)
elseif (USE_BUNDLED_UTF8PROC STREQUAL "FALSE")
 message (STATUS "Enabling utf8proc")
  find_package(PkgConfig)
  pkg_check_modules(PC_UTF8PROC REQUIRED libutf8proc)
  add_library(utf8proc STATIC IMPORTED)
  set_target_properties(utf8proc 
                        PROPERTIES
                        COMPILE_OPTIONS ${PC_UTF8PROC_STATIC_CFLAGS} 
                        LINK_LIBRARIES ${PC_UTF8PROC_LINK_LIBRARIES} 
                        INCLUDE_DIRECTORIES ${PC_UTF8PROC_STATIC_LIBRARY_DIRS} 
                        IMPORTED_LOCATION ${PC_UTF8PROC_LINK_LIBRARIES})
else ()
   message (FATAL_ERROR
            "USE_BUNDLED_UTF8PROC must be set to either TRUE or FALSE"
            )
endif()