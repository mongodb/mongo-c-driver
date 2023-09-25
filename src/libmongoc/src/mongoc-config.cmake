include(CMakeFindDependencyMacro)
find_dependency(bson-1.0 @libmongoc_VERSION@)
include("${CMAKE_CURRENT_LIST_DIR}/mongoc-targets.cmake")

unset(_required)
unset(_quiet)
if(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED)
  set(_required REQUIRED)
endif()
if(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
  set(_quiet QUIET)
endif()

set(_mongoc_built_with_bundled_utf8proc "@USE_BUNDLED_UTF8PROC@")
if(NOT _mongoc_built_with_bundled_utf8proc AND NOT TARGET PkgConfig::PC_UTF8PROC)
  # libmongoc was compiled against an external utf8proc and links against a
  # FindPkgConfig-generated IMPORTED target. Find that package and generate that
  # imported target here:
  find_dependency(PkgConfig)
  pkg_check_modules(PC_UTF8PROC ${_required} ${_quiet} libutf8proc IMPORTED_TARGET GLOBAL)
endif()

# Find dependencies for SASL
set(_sasl_backend [[@SASL_BACKEND@]])
if(_sasl_backend STREQUAL "Cyrus")
  # We need libsasl2. The find-module should be installed within this package.
  # temporarily place it on the module search path:
  set(_prev_path "${CMAKE_MODULE_PATH}")
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/3rdParty")
  find_dependency(SASL2 2.0)
  set(CMAKE_MODULE_PATH "${_prev_path}")
endif()
