include(CMakeFindDependencyMacro)
find_dependency(bson-1.0 @MONGOC_MAJOR_VERSION@.@MONGOC_MINOR_VERSION@.@MONGOC_MICRO_VERSION@)
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
if(NOT TARGET _mongoc::detail::sasl AND _sasl_backend)
  add_library(_mongoc::detail::sasl IMPORTED INTERFACE)
endif()
set(_sasl_sspi_libraries [[@SASL_SSPI_LIBRARIES@]])
if(_sasl_backend STREQUAL "Cyrus")
  # We need libsasl2. At build-time, we use FindSASL2.cmake, but that will probably
  # not be available. Instead, replicate the minimum logic to define our own
  # SASL2::SASL2 IMPORTED target to satisfy the dependency
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(_MONGOC_LIBSASL2 ${_quiet} IMPORTED_TARGET GLOBAL libsasl2)
  endif()
  if(_MONGOC_LIBSASL2_FOUND)
    target_link_libraries(_mongoc::detail::sasl INTERFACE PkgConfig::_MONGOC_LIBSASL2)
  else()
    # We don't have the full libsasl2 dev package, but we were compiled against one.
    # Do the next best thing: Find the shared library file and use that for our imported target.
    find_library(
      _MONGOC_LIBSASL2_DYNAMIC_LIBRARY
      NAMES
        sasl2
        ${CMAKE_SHARED_LIBRARY_PREFIX}sasl2${CMAKE_SHARED_LIBRARY_SUFFIX}.3
        ${CMAKE_SHARED_LIBRARY_PREFIX}sasl2${CMAKE_SHARED_LIBRARY_SUFFIX}.2
        ${CMAKE_SHARED_LIBRARY_PREFIX}sasl2${CMAKE_SHARED_LIBRARY_SUFFIX}.1
        ${CMAKE_SHARED_LIBRARY_PREFIX}sasl2${CMAKE_SHARED_LIBRARY_SUFFIX}
      DOC "Path to a Cyrus libsasl2 library for linking against with libmongoc"
      )
    mark_as_advanced(_MONGOC_LIBSASL2_DYNAMIC_LIBRARY)
    if(_MONGOC_LIBSASL2_DYNAMIC_LIBRARY)
      target_link_libraries(_mongoc::detail::sasl INTERFACE "${_MONGOC_LIBSASL2_DYNAMIC_LIBRARY}")
    else()
      #[[
        Worst case: No libsasl2.pc, no libsasl2.so, and no suffixed version.
        We're okay in most cases though, because the dynamic library will contain
        a runtime link dep on the sasl2 library.
      ]]
    endif()
  endif()
elseif(_sasl_backend STREQUAL "SSPI")
  target_link_libraries(_mongoc::detail::sasl INTERFACE ${_sasl_sspi_libraries})
endif()
