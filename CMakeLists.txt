cmake_minimum_required (VERSION 3.15)

project (mongo-c-driver C)

list (APPEND CMAKE_MODULE_PATH
      "${PROJECT_SOURCE_DIR}/build/cmake"
      "${PROJECT_SOURCE_DIR}/build/cmake/make_dist"
      )

include (MongoSettings)

# Subcomponents:
mongo_bool_setting(ENABLE_MONGOC "Enable the build of libmongoc libraries (The MongoDB C database driver)")
mongo_bool_setting(
   USE_SYSTEM_LIBBSON [[Use an existing libbson library (via find_package()) instead of building one from source]]
   DEFAULT VALUE OFF
   ADVANCED
)

# Deprecated option "ENABLE_BSON" is gone, but old caches may rely on SYSTEM to
# disable the libbson build. Allow ENABLE_BSON=SYSTEM only if USE_SYSTEM_LIBBSON
# is also true, to allow both ENABLE_BSON=SYSTEM and USE_SYSTEM_LIBBSON=TRUE to
# simplify downstream build scripts
if(ENABLE_BSON STREQUAL "SYSTEM" AND NOT USE_SYSTEM_LIBBSON)
   # User set ENABLE_BSON=SYSTEM, so they intend for us to build against an external
   # libbson, but they didn't set USE_SYSTEM_LIBBSON=ON, so they are probably
   # unaware of this change. Rather than miscompile, stop what we're doing:
   message(FATAL_ERROR "ENABLE_BSON=SYSTEM is no longer supported. Use USE_SYSTEM_LIBBSON=ON for the same effect.")
endif()

# Static libraries:
mongo_setting(
   ENABLE_STATIC "Enable building and installing static library archive artifacts"
   OPTIONS ON OFF BUILD_ONLY
   DEFAULT VALUE "ON"
)
if(NOT WIN32)
   mongo_bool_setting(ENABLE_PIC "Enable position-independent code when building static libraries (Not applicable on Windows)")
elseif(DEFINED ENABLE_PIC)
   # We are on Windows, and ENABLE_PIC is defined.
   message(STATUS "ENABLE_PIC=“${ENABLE_PIC}” has no effect on Windows (All Windows code is position-independent)")
else()
   # We are on Windows, and ENABLE_PIC is not defined, which is normal
endif()

# Dev mode checks:
mongo_bool_setting(
   ENABLE_MAINTAINER_FLAGS "Enable stricter build-time checks"
   DEFAULT VALUE OFF
      DEVEL EVAL [[
         if(MSVC)
            set(DEFAULT OFF)
         else()
            set(DEFAULT ON)
         endif()
      ]])

# Toggle instrumentation:
mongo_bool_setting(ENABLE_TRACING "Enable runtime tracing output in the built libmongoc (Very noisy!)"
                   DEFAULT VALUE OFF)
mongo_bool_setting(ENABLE_COVERAGE "Build code with coverage instrumentation"
                   DEFAULT VALUE OFF DEVEL VALUE ON)
mongo_bool_setting(ENABLE_DEBUG_ASSERTIONS "Build library with runtime debug assertions enabled"
                   DEFAULT VALUE OFF DEVEL VALUE ON)
# for MONGO_SANITIZE:
include(Sanitizers)

# Toggle optional components:
mongo_bool_setting(ENABLE_TESTS "Build MongoDB C Driver tests")
mongo_bool_setting(ENABLE_EXAMPLES "Build MongoDB C Driver examples")
mongo_bool_setting(ENABLE_MAN_PAGES "Build the manual pages" DEFAULT VALUE OFF)
mongo_bool_setting(ENABLE_HTML_DOCS "Build the HTML documentation" DEFAULT VALUE OFF)
mongo_bool_setting(ENABLE_UNINSTALL "Generate an 'uninstall' script and an 'uninstall' build target")

# Optional features that are ENABLED when necessary dependencies are found:
mongo_setting(ENABLE_SRV "Enable support for mongodb+srv URIs."
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO)
mongo_setting(ENABLE_SNAPPY "Enable Snappy compression support"
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO)
mongo_setting(ENABLE_ZSTD "Enable zstd compression support"
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO)
mongo_setting(ENABLE_ZLIB "Enable zlib compression support"
              OPTIONS BUNDLED SYSTEM OFF
              DEFAULT VALUE BUNDLED)
mongo_setting(
   ENABLE_SSL [[Enable TLS connection and SCRAM authentication.]]
   OPTIONS WINDOWS DARWIN OPENSSL LIBRESSL OFF AUTO
   DEFAULT VALUE AUTO
   VALIDATE CODE [[
      if(ENABLE_SSL STREQUAL "DARWIN" AND NOT APPLE)
         message(WARNING "ENABLE_SSL=DARWIN is only supported on Apple platforms")
      elseif(ENABLE_SSL STREQUAL "WINDOWS" AND NOT WIN32)
         message(WARNING "ENABLE_SSL=WINDOWS is only supported on Windows platforms")
      endif()
   ]]
)
mongo_bool_setting(
   ENABLE_SHM_COUNTERS "Enable memory performance counters"
   DEFAULT EVAL [[
      set(DEFAULT OFF)
      if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" OR (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
         set(DEFAULT ON)
      endif()
   ]]
   VALIDATE CODE [[
      if(ENABLE_SHM_COUNTERS
         AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
         AND NOT (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
         message(WARNING "ENABLE_SHM_COUNTERS=ON is only supported on Linux or ARM Darwin")
      endif()
   ]]
)

mongo_setting(
   ENABLE_SASL "Enable SASL authentication (Kerberos)"
   OPTIONS CYRUS SSPI OFF AUTO
   DEFAULT VALUE AUTO
   VALIDATE CODE [[
      if(ENABLE_SASL STREQUAL "SSPI" AND NOT WIN32)
         message(WARNING "ENABLE_SASL=SSPI is only supported on Windows platforms")
      endif()
   ]]
)

mongo_setting(ENABLE_ICU "Enable ICU support, needed for non-ASCII usernames and passwords"
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO)
mongo_setting(ENABLE_CLIENT_SIDE_ENCRYPTION "Enable In-Use Encryption support. Requires additional support libraries."
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO)
mongo_setting(ENABLE_MONGODB_AWS_AUTH "Enable support for the MONGODB-AWS authentication mechanism"
              OPTIONS ON OFF AUTO
              DEFAULT VALUE AUTO
              VALIDATE CODE [[
                  if(NOT ENABLE_MONGODB_AWS_AUTH STREQUAL "ON")
                     return()
                  endif()
                  if(MSVC AND MSVC_VERSION LESS 1900)
                     message(FATAL_ERROR "Visual Studio 2015 or newer is required for ENABLE_MONGODB_AWS_AUTH=ON")
                  elseif(ENABLE_SSL STREQUAL "OFF")
                     message(FATAL_ERROR "ENABLE_MONGODB_AWS_AUTH=ON requires that ENABLE_SSL not be 'OFF'")
                  endif()
              ]])

# Optional features that are DISABLED by default:
mongo_bool_setting(ENABLE_RDTSCP "Enable fast performance counters using the Intel RDTSCP instruction"
                   DEFAULT VALUE OFF)
mongo_bool_setting(ENABLE_APPLE_FRAMEWORK "Build libraries as Apple Frameworks on Darwin platforms"
                   DEFAULT VALUE OFF)
mongo_bool_setting(
   ENABLE_CRYPTO_SYSTEM_PROFILE "Use system crypto profile"
   DEFAULT VALUE OFF
   VALIDATE CODE [[
      if(ENABLE_CRYPTO_SYSTEM_PROFILE AND NOT ENABLE_SSL STREQUAL "OPENSSL")
         message(WARNING "ENABLE_CRYPTO_SYSTEM_PROFILE=TRUE is only applicable when ENABLE_SSL=OPENSSL")
      endif()
   ]]
)

# Deprecated options:
mongo_bool_setting(
   ENABLE_AUTOMATIC_INIT_AND_CLEANUP
      "[Deprecated] Enable automatic initialization of the C driver library"
   DEFAULT EVAL [[
         set(DEFAULT OFF)
         if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            message(VERBOSE "For backwards compatibility, when using GCC the default of "
                            "ENABLE_AUTOMATIC_INIT_AND_CLEANUP is ON")
            set(DEFAULT ON)
         endif()
      ]]
      DEVEL VALUE OFF
   VALIDATE CODE [[
      if(ENABLE_AUTOMATIC_INIT_AND_CLEANUP)
         if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
            message(WARNING "ENABLE_AUTOMATIC_INIT_AND_CLEANUP is only supported on GCC.")
         endif()
         message(DEPRECATION
                  "Enabling ENABLE_AUTOMATIC_INIT_AND_CLEANUP is deprecated and "
                  "may be removed in a future release")
      endif()
   ]]
)
mongo_bool_setting(
   ENABLE_EXTRA_ALIGNMENT
      "[Deprecated] Enable extra alignment on libbson types"
   DEFAULT VALUE ON
      DEVEL VALUE OFF
   VALIDATE CODE [[
      if(ENABLE_EXTRA_ALIGNMENT AND MONGO_SANITIZE MATCHES "undefined")
         message(WARNING "ENABLE_EXTRA_ALIGNMENT=“${ENABLE_EXTRA_ALIGNMENT}” will create conflicts with UndefinedBehaviorSanitizer")
      endif()
   ]]
)

# Optionally enable C++ to do some C++-specific tests
include (CheckLanguage)
check_language (CXX)
if (CMAKE_CXX_COMPILER)
   enable_language (CXX)
   if (NOT CMAKE_CXX_STANDARD)
      # Default to C++11 for purposes of testing.
      set (CMAKE_CXX_STANDARD 11)
   endif ()
else ()
   message (STATUS "No CXX support")
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
   set (CMAKE_BUILD_TYPE "RelWithDebInfo")
   message (
      STATUS "No CMAKE_BUILD_TYPE selected, defaulting to ${CMAKE_BUILD_TYPE}"
   )
endif ()

include (InstallRequiredSystemLibraries)
include (GNUInstallDirs)
include (CMakeDependentOption)

include(MongoC-Warnings)

# Enable CCache, if possible
include (CCache)

# Link with LLD, if possible
if (NOT MSVC)
   include (LLDLinker)
endif ()

set (BUILD_VERSION "0.0.0" CACHE STRING "Library version (for both libbson and libmongoc)")

include (ParseVersion)

# Set MONGOC_MAJOR_VERSION, MONGOC_MINOR_VERSION, etc.
if (BUILD_VERSION STREQUAL "0.0.0")
   if (EXISTS ${PROJECT_SOURCE_DIR}/VERSION_CURRENT)
      file (STRINGS ${PROJECT_SOURCE_DIR}/VERSION_CURRENT BUILD_VERSION)
      message (STATUS "file VERSION_CURRENT contained BUILD_VERSION ${BUILD_VERSION}")
   else ()
      find_package (PythonInterp)
      if (PYTHONINTERP_FOUND)
         execute_process (
            COMMAND ${PYTHON_EXECUTABLE} build/calc_release_version.py
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE CALC_RELEASE_VERSION
            RESULT_VARIABLE CALC_RELEASE_VERSION_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
         )
         if (NOT CALC_RELEASE_VERSION_RESULT STREQUAL 0)
            # If python failed above, stderr would tell the user about it
            message (FATAL_ERROR
               "BUILD_VERSION not specified and could not be calculated\
 (script invocation failed); specify in CMake command, -DBUILD_VERSION=<version>"
            )
         else ()
            set (BUILD_VERSION ${CALC_RELEASE_VERSION})
            message (STATUS "calculated BUILD_VERSION ${BUILD_VERSION}")
         endif ()
      else ()
         message (FATAL_ERROR
            "BUILD_VERSION not specified and could not be calculated\
 (Python was not found on the system); specify in CMake command, -DBUILD_VERSION=<version>"
         )
      endif ()
      message (STATUS "storing BUILD_VERSION ${BUILD_VERSION} in file VERSION_CURRENT for later use")
      file (WRITE ${PROJECT_SOURCE_DIR}/VERSION_CURRENT ${BUILD_VERSION})
   endif ()
else ()
   message (STATUS "storing BUILD_VERSION ${BUILD_VERSION} in file VERSION_CURRENT for later use")
   file (WRITE ${PROJECT_SOURCE_DIR}/VERSION_CURRENT ${BUILD_VERSION})
endif ()

include (LoadVersion)
LoadVersion (${PROJECT_SOURCE_DIR}/VERSION_CURRENT MONGOC)

# Enable "maintainer flags," which are supplementary but not mandatory.
# (As opposed to MongoC-Warnings.cmake, which are required warnings)
include (MaintainerFlags)

if ( (ENABLE_BUILD_DEPENDECIES STREQUAL OFF) AND (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR) )
   set (ENABLE_BUILD_DEPENDECIES ON)
endif ()

_mongo_pick(BSON_EXTRA_ALIGN 1 0 ENABLE_EXTRA_ALIGNMENT)

_mongo_pick(MONGOC_ENABLE_RDTSCP 1 0 ENABLE_RDTSCP)

_mongo_pick(MONGOC_ENABLE_STATIC_BUILD 1 0 ENABLE_STATIC)
_mongo_pick(MONGOC_ENABLE_STATIC_INSTALL 1 0
            [[ENABLE_STATIC AND NOT ENABLE_STATIC STREQUAL "BUILD_ONLY"]])

if (USE_SYSTEM_LIBBSON)
   # The input variable BSON_ROOT_DIR is respected for backwards compatibility,
   # but you should use the standard CMAKE_PREFIX_PATH instead.
   message (STATUS "Searching for libbson CMake packages")
   find_package (bson-1.0
      "${MONGOC_MAJOR_VERSION}.${MONGOC_MINOR_VERSION}.${MONGOC_MICRO_VERSION}"
      HINTS
      ${BSON_ROOT_DIR})

   if (NOT bson-1.0_FOUND)
      message (FATAL_ERROR "System libbson not found")
   endif ()

   message (STATUS "libbson found version \"${bson-1.0_VERSION}\"")
   message (STATUS "disabling test-libmongoc since using system libbson")
   SET (ENABLE_TESTS OFF)

   set (USING_SYSTEM_BSON TRUE)
   if (NOT TARGET mongo::bson_shared)
           message (FATAL_ERROR "System libbson built without shared library target")
   endif ()
   set (BSON_LIBRARIES mongo::bson_shared)
   if (NOT TARGET mongo::bson_static)
           message (FATAL_ERROR "System libbson built without static library target")
   endif ()
   set (BSON_STATIC_LIBRARIES mongo::bson_static)
endif ()

unset (dist_generated CACHE)
unset (dist_generated_depends CACHE)

set (BUILD_SOURCE_DIR ${CMAKE_BINARY_DIR})

include (MakeDistFiles)

# Enable CTest
include (CTest)
if (BUILD_TESTING)
   include (TestFixtures)
endif ()

# Ensure the default behavior: don't ignore RPATH settings.
set (CMAKE_SKIP_BUILD_RPATH OFF)

# Ensure the default behavior: don't use the final install destination as the
# temporary RPATH for executables (ensure we can run tests and programs from
# the build directory).
set (CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

# Include any custom library paths in the final RPATH.
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

# Install libs with names like @rpath/libmongoc-1.0.0.dylib, not bare names.
set (CMAKE_MACOSX_RPATH ON)

# https://cmake.org/cmake/help/v3.11/policy/CMP0042.html
# Enable a CMake 3.0+ policy that sets CMAKE_MACOSX_RPATH by default, and
# silence a CMake 3.11 warning that the old behavior is deprecated.
cmake_policy (SET CMP0042 NEW)

# By default, ensure conformance with a minimum C standard.
# Required extensions to the language (i.e. POSIX) are (re)enabled further below.
if (NOT DEFINED CMAKE_C_STANDARD)
   set (CMAKE_C_STANDARD 99)
endif ()
if (NOT DEFINED CMAKE_C_STANDARD_REQUIRED)
   set (CMAKE_C_STANDARD_REQUIRED ON)
endif ()
if (NOT DEFINED CMAKE_C_EXTENSIONS)
   set (CMAKE_C_EXTENSIONS OFF)
endif ()

# https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
# https://pubs.opengroup.org/onlinepubs/7908799/xsh/compilation.html
# Enable POSIX features up to POSIX.1-2008 plus the XSI extension and BSD-derived definitions.
# Both _BSD_SOURCE and _DEFAULT_SOURCE are defined for backwards-compatibility with glibc 2.19 and earlier.
# _BSD_SOURCE and _DEFAULT_SOURCE are required by `getpagesize`, `h_errno`, etc.
# _XOPEN_SOURCE=700 is required by `strnlen`, `strerror_l`, etc.
add_definitions (-D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE)
list (APPEND CMAKE_REQUIRED_DEFINITIONS -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE)

# Enable non-standard features on FreeBSD with __BSD_VISIBLE=1
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   add_definitions (-D__BSD_VISIBLE=1)
   list (APPEND CMAKE_REQUIRED_DEFINITIONS -D__BSD_VISIBLE=1)
endif ()

# https://opensource.apple.com/source/Libc/Libc-1439.40.11/gen/compat.5.auto.html
# Non-POSIX extensions are required by `_SC_NPROCESSORS_ONLN`.
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
   add_definitions (-D_DARWIN_C_SOURCE)
   list (APPEND CMAKE_REQUIRED_DEFINITIONS -D_DARWIN_C_SOURCE)
endif ()

add_subdirectory (src/common)

if (NOT USING_SYSTEM_BSON)
   message (STATUS "Using bundled libbson")

   add_subdirectory (src/libbson)
   # Defined in src/libbson/CMakeLists.txt
   set (BSON_STATIC_LIBRARIES bson_static)
   set (BSON_LIBRARIES bson_shared)
   set (BSON_STATIC_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src/libbson/src" "${PROJECT_BINARY_DIR}/src/libbson/src")
   set (BSON_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src/libbson/src" "${PROJECT_BINARY_DIR}/src/libbson/src")
   set (BSON_STATIC_PUBLIC_DEFINITIONS "BSON_STATIC")
endif ()

if (MSVC)
   add_definitions (-D_CRT_SECURE_NO_WARNINGS)
endif ()

if (ENABLE_MONGOC)
   if (ENABLE_TESTS AND NOT MONGOC_ENABLE_STATIC_BUILD)
      message (FATAL_ERROR "ENABLE_TESTS requires ENABLE_STATIC or ENABLE_STATIC_BUILD")
   endif ()

   if (NOT ENABLE_SSL MATCHES "DARWIN|WINDOWS|OPENSSL|LIBRESSL|AUTO|OFF")
      message (FATAL_ERROR
               "ENABLE_SSL option must be DARWIN, WINDOWS, OPENSSL, LIBRESSL, AUTO, or OFF")
   endif ()

   set (SOURCE_DIR "${PROJECT_SOURCE_DIR}/")

   set (ZLIB_SOURCES
      ${SOURCE_DIR}/src/zlib-1.2.13/adler32.c
      ${SOURCE_DIR}/src/zlib-1.2.13/crc32.c
      ${SOURCE_DIR}/src/zlib-1.2.13/deflate.c
      ${SOURCE_DIR}/src/zlib-1.2.13/infback.c
      ${SOURCE_DIR}/src/zlib-1.2.13/inffast.c
      ${SOURCE_DIR}/src/zlib-1.2.13/inflate.c
      ${SOURCE_DIR}/src/zlib-1.2.13/inftrees.c
      ${SOURCE_DIR}/src/zlib-1.2.13/trees.c
      ${SOURCE_DIR}/src/zlib-1.2.13/zutil.c
      ${SOURCE_DIR}/src/zlib-1.2.13/compress.c
      ${SOURCE_DIR}/src/zlib-1.2.13/uncompr.c
      ${SOURCE_DIR}/src/zlib-1.2.13/gzclose.c
      ${SOURCE_DIR}/src/zlib-1.2.13/gzlib.c
      ${SOURCE_DIR}/src/zlib-1.2.13/gzread.c
      ${SOURCE_DIR}/src/zlib-1.2.13/gzwrite.c
   )

   set (MONGOC_ENABLE_ICU 0)

   set (CPACK_RESOURCE_FILE_LICENSE "${SOURCE_DIR}/COPYING")

   include (CPack)

   # Ensure the default behavior: don't ignore RPATH settings.
   set (CMAKE_SKIP_BUILD_RPATH OFF)

   if (APPLE)
      # Until CDRIVER-520.
      add_definitions (-Wno-deprecated-declarations)
   endif ()

   set (KMS_MSG_SOURCES
      ${SOURCE_DIR}/src/kms-message/src/hexlify.c
      ${SOURCE_DIR}/src/kms-message/src/kms_b64.c
      ${SOURCE_DIR}/src/kms-message/src/kms_caller_identity_request.c
      ${SOURCE_DIR}/src/kms-message/src/kms_crypto_apple.c
      ${SOURCE_DIR}/src/kms-message/src/kms_crypto_libcrypto.c
      ${SOURCE_DIR}/src/kms-message/src/kms_crypto_none.c
      ${SOURCE_DIR}/src/kms-message/src/kms_crypto_windows.c
      ${SOURCE_DIR}/src/kms-message/src/kms_decrypt_request.c
      ${SOURCE_DIR}/src/kms-message/src/kms_encrypt_request.c
      ${SOURCE_DIR}/src/kms-message/src/kms_kv_list.c
      ${SOURCE_DIR}/src/kms-message/src/kms_message.c
      ${SOURCE_DIR}/src/kms-message/src/kms_port.c
      ${SOURCE_DIR}/src/kms-message/src/kms_request.c
      ${SOURCE_DIR}/src/kms-message/src/kms_request_opt.c
      ${SOURCE_DIR}/src/kms-message/src/kms_request_str.c
      ${SOURCE_DIR}/src/kms-message/src/kms_response.c
      ${SOURCE_DIR}/src/kms-message/src/kms_response_parser.c
      ${SOURCE_DIR}/src/kms-message/src/sort.c
   )

   if (NOT ENABLE_MONGODB_AWS_AUTH MATCHES "ON|OFF|AUTO")
      message (FATAL_ERROR "ENABLE_MONGODB_AWS_AUTH option must be ON, AUTO, or OFF")
   endif ()
   set (MONGOC_ENABLE_MONGODB_AWS_AUTH 0)
   if (ENABLE_MONGODB_AWS_AUTH STREQUAL ON)
      if (MSVC AND MSVC_VERSION LESS 1900)
         message (FATAL_ERROR "Use Visual Studio 2015 or higher for ENABLE_MONGODB_AWS_AUTH")
      endif ()
      if (ENABLE_SSL STREQUAL OFF)
         message (FATAL_ERROR
            "Option ENABLE_MONGODB_AWS_AUTH requires ENABLE_SSL not set to OFF"
         )
      endif ()
      set (MONGOC_ENABLE_MONGODB_AWS_AUTH 1)
   elseif (ENABLE_MONGODB_AWS_AUTH STREQUAL AUTO)
      if (MSVC AND MSVC_VERSION LESS 1900)
         message (WARNING "MS Visual Studio too old for ENABLE_MONGODB_AWS_AUTH")
      elseif (ENABLE_SSL STREQUAL OFF)
              message (WARNING
            "Option ENABLE_MONGODB_AWS_AUTH requires ENABLE_SSL not set to OFF"
         )
      else ()
         set (MONGOC_ENABLE_MONGODB_AWS_AUTH 1)
      endif ()
   endif ()

   add_subdirectory (src/libmongoc)

   if (MONGOC_ENABLE_MONGODB_AWS_AUTH)
      message (STATUS "Building with MONGODB-AWS auth support")
   endif ()

   if (ENABLE_MAN_PAGES STREQUAL ON OR ENABLE_HTML_DOCS STREQUAL ON)
      find_package (Sphinx REQUIRED)
      add_custom_target (doc
         ALL
         DEPENDS
            $<$<NOT:$<BOOL:${USE_SYSTEM_LIBBSON}>>:bson-doc>
            $<$<BOOL:${ENABLE_MONGOC}>:mongoc-doc>
      )
   endif ()

   # Implement 'dist' and 'distcheck' targets
   #
   # CMake does not implement anything like 'dist' and 'distcheck' from autotools.
   # This implementation is based on the one in GnuCash.

   add_subdirectory (build)
   # sub-directory 'doc' was already included above
   add_subdirectory (orchestration_configs)
   add_subdirectory (src)
   # 'src/libbson' was already included, so 'src' will not include it directly
   # 'src/kms-message' was already included if appropriate
   # 'src/libmongoc' was already included, so 'src' will not include it directly

   set (PACKAGE_PREFIX "mongo-c-driver-${MONGOC_DIST_VERSION}")
   set (DIST_FILE "${PACKAGE_PREFIX}.tar.gz")

   set (top_DIST_local
      CMakeLists.txt
      CONTRIBUTING.md
      COPYING
      NEWS
      README.rst
      THIRD_PARTY_NOTICES
      VERSION_CURRENT
      # This sub-directory is added later, so manually include here
      generate_uninstall/CMakeLists.txt
   )

   set_local_dist (top_DIST ${top_DIST_local})

   set (ALL_DIST
      ${top_DIST}
      ${build_DIST}
      ${orchestration_configs_DIST}
      ${src_DIST}
      ${src_libbson_DIST}
      ${src_libmongoc_DIST}
   )

   # Write a dist manifest
   string (REPLACE ";" "\n" ALL_DIST_LINES "${ALL_DIST}")
   file (WRITE ${CMAKE_BINARY_DIR}/dist_manifest.txt ${ALL_DIST_LINES})

   # This is the command that produces the distribution tarball
   add_custom_command (OUTPUT ${DIST_FILE}
      COMMAND ${CMAKE_COMMAND}
         -D CMAKE_MODULE_PATH=${PROJECT_SOURCE_DIR}/build/cmake/make_dist
         -D PACKAGE_PREFIX=${PACKAGE_PREFIX}
         -D MONGOC_SOURCE_DIR=${CMAKE_SOURCE_DIR}
         -D BUILD_SOURCE_DIR=${BUILD_SOURCE_DIR}
         -D SHELL=${SHELL}
         "-Ddist_generated=\"${dist_generated}\""
         -P ${PROJECT_SOURCE_DIR}/build/cmake/make_dist/MakeDist.cmake

      DEPENDS
         ${ALL_DIST} ${dist_generated_depends}
   )

   if (NOT USE_SYSTEM_LIBBSON AND ENABLE_MAN_PAGES STREQUAL ON AND ENABLE_HTML_DOCS STREQUAL ON)
      # Since our 'dist' implementation does not add top-level targets for every
      # file to be included, we declare a dependency on the 'mongo-doc' target so
      # that documentation is built before the distribution tarball is generated.
      add_custom_target (dist DEPENDS doc ${DIST_FILE})

      add_custom_target (distcheck DEPENDS dist
         COMMAND ${CMAKE_COMMAND}
            -D CMAKE_MODULE_PATH=${PROJECT_SOURCE_DIR}/build/cmake/make_dist
            -D CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
            -D PACKAGE_PREFIX=${PACKAGE_PREFIX}
            -D CMAKE_C_FLAGS=${CMAKE_C_FLAGS}
            -D CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
            -P ${PROJECT_SOURCE_DIR}/build/cmake/make_dist/MakeDistCheck.cmake
      )
   else ()
      string (CONCAT DISTERRMSG
         "The dist and distcheck targets are disabled. Set USE_SYSTEM_LIBBSON=OFF, "
         "ENABLE_MAN_PAGES=ON, ENABLE_HTML_DOCS=ON, and generate VERSION_CURRENT "
         "to enable."
      )

      add_custom_target (dist
         COMMAND ${CMAKE_COMMAND} -E echo "${DISTERRMSG}"
      )

      add_custom_target (distcheck
         COMMAND ${CMAKE_COMMAND} -E echo "${DISTERRMSG}"
      )
   endif ()

endif ()

install (FILES COPYING NEWS README.rst THIRD_PARTY_NOTICES
   DESTINATION ${CMAKE_INSTALL_DATADIR}/mongo-c-driver
)

if (ENABLE_UNINSTALL)
   if (WIN32)
      if (ENABLE_MONGOC)
         set (UNINSTALL_PROG "uninstall.cmd")
      else ()
         set (UNINSTALL_PROG "uninstall-bson.cmd")
      endif ()
   else ()
      if (ENABLE_MONGOC)
         set (UNINSTALL_PROG "uninstall.sh")
      else ()
         set (UNINSTALL_PROG "uninstall-bson.sh")
      endif ()
   endif ()
   set (UNINSTALL_PROG_DIR "${CMAKE_INSTALL_DATADIR}/mongo-c-driver")

   # Create uninstall program and associated uninstall target
   #
   # This needs to be last (after all other add_subdirectory calls) to ensure that
   # the generated uninstall program is complete and correct
   add_subdirectory (generate_uninstall)
endif ()

# Spit out some information regarding the generated build system
message (STATUS "Build files generated for:")
message (STATUS "\tbuild system: ${CMAKE_GENERATOR}")
if (CMAKE_GENERATOR_INSTANCE)
   message (STATUS "\tinstance: ${CMAKE_GENERATOR_INSTANCE}")
endif ()
if (CMAKE_GENERATOR_PLATFORM)
   message (STATUS "\tinstance: ${CMAKE_GENERATOR_PLATFORM}")
endif ()
if (CMAKE_GENERATOR_TOOLSET)
   message (STATUS "\tinstance: ${CMAKE_GENERATOR_TOOLSET}")
endif ()

if (TARGET test-libmongoc)
   # Generate a file that can be included by CTest to load and enumerate all of the
   # tests defined by the test-libmongoc executable. Generate one for each
   # configuration in case of multiconf generators.
   string (CONFIGURE [=[
      set (TEST_LIBMONGOC_EXE [[$<TARGET_FILE:test-libmongoc>]])
      set (SRC_ROOT [[@PROJECT_SOURCE_DIR@]])
      set (IS_MULTICONF $<BOOL:@CMAKE_CONFIGURATION_TYPES@>)
      if (NOT IS_MULTICONF OR CTEST_CONFIGURATION_TYPE STREQUAL "$<CONFIG>")
         # We are not in multi-conf, or the current config matches our config.
         include ("${SRC_ROOT}/build/cmake/LoadTests.cmake")
      elseif (NOT CTEST_CONFIGURATION_TYPE)
         # We are in multi-conf, but no '-C' config was specified
         message (WARNING "Specify a --build-config when using CTest with a multi-config build")
      else ()
         # Do nothing. Not our config.
      endif ()
   ]=] code @ONLY)
   file (GENERATE
         OUTPUT "${PROJECT_BINARY_DIR}/LoadTests-$<CONFIG>.cmake"
         CONTENT "${code}")
   if (CMAKE_CONFIGURATION_TYPES)
      foreach (conf IN LISTS CMAKE_CONFIGURATION_TYPES)
         # Direct the generated CTest code to include() the file that loads the tests:
         set_property (
            DIRECTORY
            APPEND PROPERTY
            TEST_INCLUDE_FILES "${PROJECT_BINARY_DIR}/LoadTests-${conf}.cmake")
      endforeach ()
   else ()
      set_property (
         DIRECTORY
         APPEND PROPERTY
         TEST_INCLUDE_FILES "${PROJECT_BINARY_DIR}/LoadTests-${CMAKE_BUILD_TYPE}.cmake")
   endif ()
endif ()

if (CMAKE_GENERATOR STREQUAL "Ninja Multi-Config" AND PROJECT_IS_TOP_LEVEL)
   set (CMAKE_CROSS_CONFIGS "all")
endif ()
