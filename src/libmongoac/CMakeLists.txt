cmake_minimum_required (VERSION 3.25...4.99)

set (version_current "${CMAKE_CURRENT_SOURCE_DIR}/VERSION_CURRENT")
if (EXISTS "${version_current}")
   file (READ "${version_current}" mongoac_version_raw)
   string (STRIP "${mongoac_version_raw}" mongoac_version_raw)
else ()
   message (FATAL_ERROR "src/libmongoac/VERSION_CURRENT is missing")
endif ()

set (MONGOAC_VERSION_FULL "${mongoac_version_raw}")
string (REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" MONGOAC_VERSION "${MONGOAC_VERSION_FULL}")
if (NOT MONGOAC_VERSION)
   message (FATAL_ERROR "src/libmongoac/VERSION_CURRENT does not contain a valid SemVer string: ${MONGOAC_VERSION_FULL}")
endif ()

project (mongoac
  LANGUAGES C
  VERSION "${MONGOAC_VERSION}"
  DESCRIPTION "The MongoDB Async C Driver library"
)

message (STATUS "mongoac version: ${MONGOAC_VERSION_FULL}")

set (MONGOAC_OUTPUT_BASENAME "mongoac" CACHE STRING "Output mongoac library base name")

# Support standalone configuration when the mongo-c-driver parent project is not used.
if (NOT DEFINED CACHE{ENABLE_SHARED})
  set (ENABLE_SHARED ON CACHE BOOL "Enable building and installing dynamic library artifacts")
endif ()
if (NOT DEFINED CACHE{ENABLE_STATIC})
  set (ENABLE_STATIC ON CACHE STRING "Enable building and installing static library archive artifacts")
endif ()
if (NOT "$CACHE{ENABLE_STATIC}" MATCHES "^(ON|BUILD_ONLY|OFF)$")
  message (FATAL_ERROR "ENABLE_STATIC must be ON, BUILD_ONLY, or OFF")
endif ()

# Building mongoac requires a Rust toolchain.
find_program (CARGO_EXECUTABLE cargo REQUIRED)

# Minimum compiler versions.
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # https://gcc.gnu.org/releases.html
  if (CMAKE_C_COMPILER_VERSION VERSION_LESS "8.1")
    message (FATAL_ERROR "GCC 8.1 or newer is required")
  endif ()
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  # https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B#Internal_version_numbering
  # https://learn.microsoft.com/en-us/cpp/overview/compiler-versions
  if (CMAKE_C_COMPILER_VERSION VERSION_LESS "19.16.27023")
    message (FATAL_ERROR "Visual Studio 2017 version 15.9 or newer is required")
  endif ()
elseif (CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
  # https://developer.apple.com/xcode/cpp/
  # https://en.wikipedia.org/wiki/Xcode#Version_comparison_table
  # https://trac.macports.org/wiki/XcodeVersionInfo
  if (CMAKE_C_COMPILER_VERSION VERSION_LESS "13.1")
    message (FATAL_ERROR "Xcode 13.4.1 or newer is required")
  endif ()
elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
  # https://releases.llvm.org/
  # https://clang.llvm.org/
  if (CMAKE_C_COMPILER_VERSION VERSION_LESS "3.8")
    message (FATAL_ERROR "Clang 3.8 or newer is required")
  endif ()
else ()
  message (WARNING "Unknown compiler... recklessly proceeding without a version check")
endif ()

include (GNUInstallDirs)

# For consistency with mongoc installation paths.
if (NOT DEFINED MONGOAC_INSTALL_INCLUDEDIR)
  set (MONGOAC_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/mongoac-${PROJECT_VERSION}")
endif ()
if (NOT DEFINED MONGOAC_INSTALL_CMAKEDIR)
  # Support standalone CMake configuration when the mongo-c-driver parent project is not used.
  if (DEFINED MONGO_C_DRIVER_INSTALL_CMAKEDIR)
    set (MONGOAC_INSTALL_CMAKEDIR "${MONGO_C_DRIVER_INSTALL_CMAKEDIR}/mongoac-${PROJECT_VERSION}")
  else ()
    set (MONGOAC_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/mongoac-${PROJECT_VERSION}")
  endif ()
endif ()

# Direct cargo to reuse the existing CMake directory structure (source vs. binary).
set (cargo_target_dir "${CMAKE_CURRENT_BINARY_DIR}/target")
set (include_dir "${CMAKE_CURRENT_BINARY_DIR}/include")

if (MONGOAC_VERSION_FULL MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+-([^+]+)")
   string (STRIP "${CMAKE_MATCH_1}" MONGOAC_VERSION_PRERELEASE)
else ()
   set (MONGOAC_VERSION_PRERELEASE "")
endif ()

configure_file (
   "${CMAKE_CURRENT_SOURCE_DIR}/include/mongoac/version.h.in"
   "${include_dir}/mongoac/version.h"
)
set (soname "$CACHE{MONGOAC_OUTPUT_BASENAME}${PROJECT_VERSION_MAJOR}")

set_source_files_properties ("${include_dir}/mongoac/version.h" PROPERTIES LANGUAGE C)

set (mongoac_file_set_headers
  FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include"
    FILES "${include_dir}/mongoac/version.h"
)

if ("$CACHE{ENABLE_SHARED}")
  add_library (mongoac_shared INTERFACE)
  set_target_properties (mongoac_shared PROPERTIES
    EXPORT_NAME mongoac::shared
    SOVERSION "${PROJECT_VERSION_MAJOR}"
  )
  target_sources (mongoac_shared INTERFACE ${mongoac_file_set_headers})
  add_library (mongoac::shared ALIAS mongoac_shared)

  install (TARGETS mongoac_shared EXPORT mongoac-targets
    FILE_SET HEADERS DESTINATION "${MONGOAC_INSTALL_INCLUDEDIR}"
  )
endif ()

if (NOT "$CACHE{ENABLE_STATIC}" STREQUAL "OFF")
  add_library (mongoac_static INTERFACE)
  set_target_properties (mongoac_static PROPERTIES EXPORT_NAME mongoac::static)
  target_compile_definitions (mongoac_static INTERFACE MONGOAC_STATIC)
  target_sources (mongoac_static INTERFACE ${mongoac_file_set_headers})
  add_library (mongoac::static ALIAS mongoac_static)

  if ("$CACHE{ENABLE_STATIC}" STREQUAL "ON")
    install (TARGETS mongoac_static EXPORT mongoac-targets
      FILE_SET HEADERS DESTINATION "${MONGOAC_INSTALL_INCLUDEDIR}"
    )
  endif ()
endif ()

if ("$CACHE{ENABLE_SHARED}" OR "$CACHE{ENABLE_STATIC}" STREQUAL "ON")
  install (EXPORT mongoac-targets
    FILE mongoac-targets.cmake
    DESTINATION "${MONGOAC_INSTALL_CMAKEDIR}"
  )
endif ()

add_subdirectory (cmake)

if (ENABLE_MAN_PAGES OR ENABLE_HTML_DOCS)
   find_package (Sphinx REQUIRED)
   add_subdirectory (doc)
   add_custom_target (mongoac-doc
      ALL
      DEPENDS
      $<TARGET_NAME_IF_EXISTS:mongoac-man>
      $<TARGET_NAME_IF_EXISTS:mongoac-html>
   )
endif ()
