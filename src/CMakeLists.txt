# sub-directory 'libbson' was already included at the top-level
# sub-directory 'libmongoc' was already included at the top-level
# sub-directory 'kms-message' was already included at the top-level

# Copy zconf.h.in to zconf.h, used by zlib
configure_file (
   "${SOURCE_DIR}/src/zlib-1.3.1/zconf.h.in"
   "${CMAKE_BINARY_DIR}/src/zlib-1.3.1/zconf.h"
   COPYONLY
)

if (CMAKE_CXX_COMPILER)
   # Add a C++ source file that will #include the main C headers. This "library"
   # does nothing other than validate that the C headers are valid C++ headers.
   add_library (mongoc-cxx-check OBJECT cpp-check.cpp)
   if (TARGET mongoc_static)
      target_link_libraries (mongoc-cxx-check PRIVATE mongoc_static)
   else ()
      target_link_libraries (mongoc-cxx-check PRIVATE mongoc_shared)
   endif ()
endif ()

# public-header-warnings
add_executable(public-header-warnings EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/c-check.c)

target_include_directories(public-header-warnings PRIVATE
    ${CMAKE_BINARY_DIR}/../install-dir/include/libbson-1.0
    ${CMAKE_BINARY_DIR}/../install-dir/include/libmongoc-1.0
)

target_compile_options(public-header-warnings PRIVATE
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Werror=all>
  $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Werror=extra>
  $<$<C_COMPILER_ID:MSVC>:/W3>
  $<$<C_COMPILER_ID:MSVC>:/WX>
  $<$<C_COMPILER_ID:MSVC>:/wd5105>
)

