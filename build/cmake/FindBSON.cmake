# Read-Only variables:
#  BSON_FOUND - system has the BSON library
#  BSON_INCLUDE_DIRS, BSON_STATIC_INCLUDE_DIRS - the BSON include directory
#  BSON_LIBRARIES, BSON_STATIC_LIBRARIES - The libraries needed to use BSON
#  BSON_VERSION, BSON_STATIC_VERSION - This is set to $major.$minor.$revision
# (eg. 0.4.1)

# The input variable BSON_ROOT_DIR is respected for backwards compatibility,
# but you should use the standard CMAKE_PREFIX_PATH instead.
message (STATUS "Searching for libbson CMake package")

find_package (libbson-1.0
   "${MONGOC_MAJOR_VERSION}.${MONGOC_MINOR_VERSION}.${MONGOC_MICRO_VERSION}"
   HINTS
     ${BSON_ROOT_DIR})

find_package (libbson-static-1.0
   "${MONGOC_MAJOR_VERSION}.${MONGOC_MINOR_VERSION}.${MONGOC_MICRO_VERSION}"
   HINTS
     ${BSON_ROOT_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
   BSON
   REQUIRED_VARS
   BSON_INCLUDE_DIRS
   BSON_LIBRARY
   BSON_STATIC_INCLUDE_DIRS
   BSON_STATIC_LIBRARY)
