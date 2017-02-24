# Read-Only variables:
#  BSON_FOUND - system has the BSON library
#  BSON_INCLUDE_DIRS - the BSON include directory
#  BSON_LIBRARIES - The libraries needed to use BSON
#  BSON_VERSION - This is set to $major.$minor.$revision$path (eg. 0.4.1)


# The input variable BSON_ROOT_DIR is respected for backwards compatibility,
# but you should use the standard CMAKE_PREFIX_PATH instead.
message (STATUS "Searching for libbson CMake package")
find_package (libbson-1.0
   "${MONGOC_MAJOR_VERSION}.${MONGOC_MINOR_VERSION}.${MONGOC_MICRO_VERSION}"
   HINTS
     ${BSON_ROOT_DIR})

if (NOT BSON_VERSION)
   find_package(PkgConfig QUIET)
   if (PKG_CONFIG_FOUND)
      pkg_search_module (BSON libbson-1.0)
   else ()
      # No CMake package installed for libbson - was it installed with the Autotools
      # or a system package manager?
      message ("Can't find libbson-config.cmake, nor pkg-config, searching for libbson-1.0/bson.h")
      find_path(BSON_INCLUDE_DIR
        NAMES
          libbson-1.0/bson.h
        HINTS
          ${BSON_ROOT_DIR}
        PATH_SUFFIXES
          include
      )

      if (NOT BSON_INCLUDE_DIR)
         message (FATAL_ERROR "Could not find libbson-1.0/bson.h")
      endif ()

      set (BSON_INCLUDE_DIRS "${BSON_INCLUDE_DIR}/libbson-1.0")

      file (STRINGS "${BSON_INCLUDE_DIR}/bson-version.h" bson_version_s
            REGEX "^#define[\t ]+BSON_VERSION_S[\t ]+\"\(.*)\"$")

      string (REGEX REPLACE "^#define[\t ]+BSON_VERSION_S[\t ]+\"\(.*)\"$"
              "\\1" BSON_VERSION "${bson_version_s}")

      find_library(BSON_LIBRARIES
         NAMES
            "bson-1.0"
         HINTS
            ${BSON_ROOT_DIR}
            PATH_SUFFIXES
            bin
            lib
      )
   endif ()
endif ()

include(FindPackageHandleStandardArgs)

if (BSON_VERSION)
   set (BSON "${BSON_LIBRARIES}")
   message ("--   Found version \"${BSON_VERSION}\"")
   message ("--   Include path \"${BSON_INCLUDE_DIRS}\"")
   message ("--   Library path \"${BSON_LIBRARY_DIRS}\"")
else ()
   find_package_handle_standard_args(BSON "Could NOT find BSON"
      BSON_VERSION
      BSON_LIBRARIES
      BSON_INCLUDE_DIRS
  )
endif ()

mark_as_advanced(BSON_INCLUDE_DIRS BSON_LIBRARIES)

if(WIN32)
   set (BSON_LIBRARIES ${BSON} ws2_32)
else()
   find_package (Threads REQUIRED)
   set (BSON_LIBRARIES ${BSON} ${CMAKE_THREAD_LIBS_INIT})
endif()
