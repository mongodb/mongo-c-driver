#[[
    This is a transitional CMake package config file to allow compatibility
    between libbson 1.x and libbson 2.x CMake packages.

    When distributed with libbson 1.x, this file is mostly just a shim for
    the `bson-1.0` CMake packages.

    This file imports "bson-1.0" and generates alias targets for it:

    • `bson::shared` → `mongo::bson_shared`
    • `bson::static` → `mongo::bson_static`
    • `bson::bson` → Points to either `bson::shared` or `bson::static`,
        controlled by `BSON_DEFAULT_IMPORTED_LIBRARY_TYPE`
]]

# Check for missing components before proceeding. We don't provide any, so we
# should generate an error if the caller requests any *required* components.
set(missing_required_components)
foreach(comp IN LISTS bson_FIND_COMPONENTS)
    if(bson_FIND_REQUIRED_${comp})
        list(APPEND missing_required_components "${comp}")
    endif()
endforeach()

if(missing_required_components)
    list(JOIN missing_required_components ", " components)
    set(bson_FOUND FALSE)
    set(bson_NOT_FOUND_MESSAGE "The package version is compatible, but is missing required components: ${components}")
    # Stop now. Don't generate any imported targets
    return()
endif()

include(CMakeFindDependencyMacro)

# We are installed alongside the `bson-1.0` packages. Forcibly prevent find_package
# from considering any other `bson-1.0` package that might live elsewhere on the system
# by setting HINTS and NO_DEFAULT_PATH
get_filename_component(parent_dir "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
find_dependency(bson-1.0 HINTS ${parent_dir} NO_DEFAULT_PATH)

# The library type that is linked with `bson::bson`
set(_default_lib_type)
# Add compat targets for the bson-1.0 package targets
if(TARGET mongo::bson_shared AND NOT TARGET bson::shared)
    add_library(bson::shared IMPORTED INTERFACE)
    set_property(TARGET bson::shared APPEND PROPERTY INTERFACE_LINK_LIBRARIES mongo::bson_shared)
    set(_default_lib_type SHARED)
endif()
if(TARGET mongo::bson_static AND NOT TARGET bson::static)
    add_library(bson::static IMPORTED INTERFACE)
    set_property(TARGET bson::static APPEND PROPERTY INTERFACE_LINK_LIBRARIES mongo::bson_static)
    # If static is available, set it as the default library type
    set(_default_lib_type STATIC)
endif()

# Allow the user to tweak what library type is linked for `bson::bson`
set(BSON_DEFAULT_IMPORTED_LIBRARY_TYPE "${_default_lib_type}"
    CACHE STRING "The default library type that is used when linking against 'bson::bson' (either SHARED or STATIC, requires that the package was built with the appropriate library type)")
set_property(CACHE BSON_DEFAULT_IMPORTED_LIBRARY_TYPE PROPERTY STRINGS SHARED STATIC)

if(NOT TARGET bson::bson)  # Don't redefine the target if we were already included
    string(TOLOWER "${BSON_DEFAULT_IMPORTED_LIBRARY_TYPE}" _type)
    add_library(bson::bson IMPORTED INTERFACE)
    set_property(TARGET bson::bson APPEND PROPERTY INTERFACE_LINK_LIBRARIES bson::${_type})
endif()
