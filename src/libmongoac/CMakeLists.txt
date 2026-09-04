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

set (cargo_target_build_dir "${cargo_target_dir}/$<IF:$<CONFIG:Debug>,debug,release>")
set (cargo_library_shared "${cargo_target_build_dir}/${CMAKE_SHARED_LIBRARY_PREFIX}$CACHE{MONGOAC_OUTPUT_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set (cargo_library_static "${cargo_target_build_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}$CACHE{MONGOAC_OUTPUT_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}")

set (soname "$CACHE{MONGOAC_OUTPUT_BASENAME}${PROJECT_VERSION_MAJOR}")
set (library_shared_filename "${CMAKE_SHARED_LIBRARY_PREFIX}${soname}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set (library_shared "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${library_shared_filename}")
set (library_shared_soversion_full "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${library_shared_filename}.${PROJECT_VERSION}")
set (library_shared_soversion "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${library_shared_filename}.${PROJECT_VERSION_MAJOR}")
set (library_static_filename "${CMAKE_STATIC_LIBRARY_PREFIX}${soname}${CMAKE_STATIC_LIBRARY_SUFFIX}")
set (library_static "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${library_static_filename}")

file (GLOB_RECURSE cargo_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rs")
file (GLOB_RECURSE mongoac_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")

set (cargo_dependencies
  ${cargo_sources}
  "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
  "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.lock"
)

set_source_files_properties (${mongoac_headers} PROPERTIES LANGUAGE C)

set (mongoac_file_set_headers
  FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include"
    FILES ${mongoac_headers}
  FILE_SET HEADERS
    BASE_DIRS "${include_dir}"
    FILES "${include_dir}/mongoac/version.h"
)

if ("$CACHE{ENABLE_SHARED}")

  add_custom_command (
    OUTPUT
      "${library_shared}"
      "${library_shared_soversion}"
      "${library_shared_soversion_full}"
    COMMENT "Building the mongoac shared library"
    COMMAND
      "${CMAKE_COMMAND}" -E env
        "MONGOAC_CMAKE_BUILD_TYPE=$<CONFIG>"
        "MONGOAC_LIBRARY_TYPE=shared"
      "${CARGO_EXECUTABLE}" rustc
        --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --locked
        --lib
        --crate-type cdylib
        --target-dir "${cargo_target_dir}"
        "$<$<NOT:$<CONFIG:Debug>>:--release>"
    COMMAND
      "${CMAKE_COMMAND}" -E copy
        "${cargo_library_shared}"
        "${library_shared_soversion_full}"
    COMMAND
      "${CMAKE_COMMAND}" -E create_symlink
        "${CMAKE_SHARED_LIBRARY_PREFIX}${soname}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJECT_VERSION}"
        "${library_shared_soversion}"
    COMMAND
      "${CMAKE_COMMAND}" -E create_symlink
        "${CMAKE_SHARED_LIBRARY_PREFIX}${soname}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJECT_VERSION_MAJOR}"
        "${library_shared}"
    ${patchelf_command}
    DEPENDS
      ${cargo_dependencies}
    BYPRODUCTS
      "${cargo_library_shared}"
  )

  add_custom_target (mongoac_shared_target ALL DEPENDS
    "${library_shared}"
    "${library_shared_soversion}"
    "${library_shared_soversion_full}"
  )

  add_library (mongoac_shared INTERFACE)
  set_target_properties (mongoac_shared PROPERTIES
    EXPORT_NAME mongoac::shared
    SOVERSION "${PROJECT_VERSION_MAJOR}"
  )
  add_dependencies (mongoac_shared mongoac_shared_target)
  target_link_libraries (mongoac_shared INTERFACE
    "$<BUILD_INTERFACE:${library_shared}>"
    "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/${library_shared_filename}>"
  )
  target_sources (mongoac_shared INTERFACE ${mongoac_file_set_headers})
  add_library (mongoac::shared ALIAS mongoac_shared)

  install (TARGETS mongoac_shared EXPORT mongoac-targets
    FILE_SET HEADERS DESTINATION "${MONGOAC_INSTALL_INCLUDEDIR}"
  )
  install (
    FILES
      "${library_shared_soversion_full}"
      "${library_shared_soversion}"
      "${library_shared}"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
endif ()

if (NOT "$CACHE{ENABLE_STATIC}" STREQUAL "OFF")
  add_custom_command (
    OUTPUT "${library_static}"
    COMMENT "Building the mongoac static library"
    COMMAND
      "${CMAKE_COMMAND}" -E env
        "MONGOAC_CMAKE_BUILD_TYPE=$<CONFIG>"
        "MONGOAC_LIBRARY_TYPE=static"
      "${CARGO_EXECUTABLE}" rustc
        --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --locked
        --lib
        --crate-type staticlib
        --target-dir "${cargo_target_dir}"
        "$<$<NOT:$<CONFIG:Debug>>:--release>"
    COMMAND
      "${CMAKE_COMMAND}" -E copy
        "${cargo_library_static}"
        "${library_static}"
    DEPENDS
      ${cargo_dependencies}
    BYPRODUCTS
      "${cargo_library_static}"
  )

  add_custom_target (mongoac_static_target ALL DEPENDS "${library_static}")

  add_library (mongoac_static INTERFACE)
  set_target_properties (mongoac_static PROPERTIES EXPORT_NAME mongoac::static)
  add_dependencies (mongoac_static mongoac_static_target)
  target_compile_definitions (mongoac_static INTERFACE MONGOAC_STATIC)
  target_link_libraries (mongoac_static INTERFACE
    "$<BUILD_INTERFACE:${library_static}>"
    "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/${library_static_filename}>"
  )
  target_sources (mongoac_static INTERFACE ${mongoac_file_set_headers})
  add_library (mongoac::static ALIAS mongoac_static)

  if ("$CACHE{ENABLE_STATIC}" STREQUAL "ON")
    install (TARGETS mongoac_static EXPORT mongoac-targets
      FILE_SET HEADERS DESTINATION "${MONGOAC_INSTALL_INCLUDEDIR}"
    )
    install (
      FILES "${library_static}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}"
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
