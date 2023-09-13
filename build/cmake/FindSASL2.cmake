#[[

Searches for a Cyrus "libsasl2" library available on the system using
pkg-config. The libsasl2.pc file must be available for pkg-config!

Upon success, Defines an imported target `SASL2::SASL2` that can be linked into
other targts.

]]

if(SASL2_FIND_COMPONENTS)
    message(FATAL_ERROR "This find_package(SASL2) does not support package components (Got “${SASL2_FIND_COMPONENTS}”)")
endif()
set(_modspec "libsasl2")
if(DEFINED SASL2_FIND_VERSION)
    set(_modspec "libsasl2>=${SASL2_FIND_VERSION}")
endif()

if(TARGET SASL2::SASL2)
    message(DEBUG "SASL2 ${SASL2_VERSION} was already found")
    # There is already a libsasl2 imported
    if(NOT DEFINED SASL2_FIND_VERISON OR SASL2_FIND_VERSION VERSION_LESS_EQUAL SASL2_VERSION)
        # Okay: We are satisfied by the version that has already been found
        set(SASL2_FOUND TRUE)
        return()
    endif()
    # Eh: The importer has requested a version of libsasl2 that is greater than the
    # version of libsasl2 that we previously imported. The pkg-config invocation
    # will likely fail, but we'll let it say that for itself.
    message(DEBUG "Need to find SASL2 again: Version requirement increased ${SASL2_FIND_VERSION} > ${SASL2_VERSION}")
endif()

# Upon early return, tell the caller that we don't have it:
set(SASL2_FOUND FALSE)

# Forward the QUIET+REQUIRED args to pkg-config
set(_required)
set(_quiet)
if(SASL2_FIND_QUIETLY)
    set(_quiet QUIET)
endif()
if(SASL2_FIND_REQUIRED)
    set(_required REQUIRED)
endif()

# We use pkg-config to find libsasl2.
find_package(PkgConfig ${_required} ${_quiet})
if(NOT PkgConfig_FOUND)
    # The find_package(PkgConfig) will have generated errors for us if we were REQUIRED,
    # so we can just return without setting any message
    set(SASL2_NOT_FOUND_MESSAGE [[No pkg-config executable was found]])
    return()
endif()

# Now ask pkg-config to find libsasl2 for us
message(DEBUG "Using pkg-config to search for module “${_modspec}”")
pkg_check_modules(libsasl2 ${_required} ${_quiet} IMPORTED_TARGET "${_modspec}" GLOBAL)
# Note: pkg_check_modules() does support static libs, but we don't use them here,
# and static libsasl2 is uncommon.
if(NOT libsasl2_FOUND)
    # The pkg_check_modules() will have issued the error for us if we were REQUIRED.
    set(SASL2_NOT_FOUND_MESSAGE [[pkg-config failed to find a libsasl2 (Cyrus) dynamic library package]])
    return()
endif()

# Instead of using the PkgConfig-generated target directly, add a level of
# indirection so that importers do not encode the dependency on the PkgConfig
# behavior. Exported users will have a link to SASL2::SASL2, but not to the
# PkgConfig target.
set(SASL2_FOUND TRUE)
add_library(SASL2::SASL2 IMPORTED INTERFACE GLOBAL)
target_link_libraries(SASL2::SASL2 INTERFACE PkgConfig::libsasl2)

set(SASL2_VERSION "${libsasl2_VERSION}" CACHE INTERNAL "")
