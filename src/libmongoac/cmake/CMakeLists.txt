include (CMakePackageConfigHelpers)

# Generate and install modern CMake package config files.
block ()
  write_basic_package_version_file (
    mongoacConfigVersion.cmake
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
  )

  configure_file (
    mongoacConfig.cmake.in
    mongoacConfig.cmake
    @ONLY
  )

  install (
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/mongoacConfigVersion.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/mongoacConfig.cmake"
    DESTINATION "${MONGOAC_INSTALL_CMAKEDIR}"
  )
endblock ()

# Generate and install pkg-config package config files.
block ()
  function (mongoac_install_pkg_config TARGET LINK_TYPE)
    set (output_name "$CACHE{MONGOAC_OUTPUT_BASENAME}")

    if (LINK_TYPE STREQUAL "STATIC")
      set (is_static 1)
      set (pcfilename "${CMAKE_CURRENT_BINARY_DIR}/${output_name}-static.pc")
    else ()
      set (is_static 0)
      set (pcfilename "${CMAKE_CURRENT_BINARY_DIR}/${output_name}.pc")
    endif ()

    add_custom_command (
      OUTPUT ${pcfilename}
      COMMAND
        ${CMAKE_COMMAND}
          -D "src_dir=${CMAKE_CURRENT_SOURCE_DIR}"
          -D "bin_dir=${CMAKE_CURRENT_BINARY_DIR}"
          -D "prefix=${CMAKE_INSTALL_PREFIX}"
          -D "includedir=${MONGOAC_INSTALL_INCLUDEDIR}"
          -D "libdir=${CMAKE_INSTALL_LIBDIR}"
          -D "output_name=${output_name}"
          -D "soname=${soname}"
          -D "version=${PROJECT_VERSION}"
          -D "is_static=${is_static}"
          -P ${CMAKE_CURRENT_SOURCE_DIR}/generate-pc.cmake
      MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/mongoac.pc.in
      DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/generate-pc.cmake
    )

    add_custom_target (generate-${TARGET}-pc ALL DEPENDS ${pcfilename})
    add_dependencies (${TARGET} generate-${TARGET}-pc)

    if (is_static)
      set (pkgname "$CACHE{MONGOAC_OUTPUT_BASENAME}${PROJECT_VERSION_MAJOR}-static.pc")
    else ()
      set (pkgname "$CACHE{MONGOAC_OUTPUT_BASENAME}${PROJECT_VERSION_MAJOR}.pc")
    endif ()

    install (
      FILES "${pcfilename}"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
      RENAME "${pkgname}"
    )
  endfunction ()

  if ("$CACHE{ENABLE_SHARED}")
    mongoac_install_pkg_config (mongoac_shared SHARED)
  endif ()

  if ("$CACHE{ENABLE_STATIC}" STREQUAL "ON")
    mongoac_install_pkg_config (mongoac_static STATIC)
  endif ()
endblock ()
