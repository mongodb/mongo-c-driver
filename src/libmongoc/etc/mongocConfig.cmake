#[[
    This is a transitional CMake package config file to allow compatibility
    between libmongoc 1.x and libmongoc 2.x CMake packages.

    When distributed with libmongoc 1.x, this file is mostly just a shim for
    the `mongoc-1.0` CMake packages.

    This file imports "mongoc-1.0" and generates alias targets for it:

    • `mongoc::shared` → `mongo::mongoc_shared`
    • `mongoc::static` → `mongo::mongoc_static`
    • `mongoc::mongoc` → Points to either `mongoc::shared` or `mongoc::static`,
        controlled by `MONGOC_DEFAULT_IMPORTED_LIBRARY_TYPE`
]]

# Check for missing components before proceeding. We don't provide any, so we
# should generate an error if the caller requests any *required* components.
set(missing_required_components)
foreach(comp IN LISTS mongoc_FIND_COMPONENTS)
    if(mongoc_FIND_REQUIRED_${comp})
        list(APPEND missing_required_components "${comp}")
    endif()
endforeach()

if(missing_required_components)
    list(JOIN missing_required_components ", " components)
    set(mongoc_FOUND FALSE)
    set(mongoc_NOT_FOUND_MESSAGE "The package version is compatible, but is missing required components: ${components}")
    # Stop now. Don't generate any imported targets
    return()
endif()

include(CMakeFindDependencyMacro)

# We are installed alongside the `mongoc-1.0` packages. Forcibly prevent find_package
# from considering any other `mongoc-1.0` package that might live elsewhere on the system
# by setting HINTS and NO_DEFAULT_PATH
get_filename_component(parent_dir "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
find_dependency(mongoc-1.0 HINTS ${parent_dir} NO_DEFAULT_PATH)
# Also import the `bson` package, to ensure its targets are also available
find_dependency(bson ${mongoc_FIND_VERSION} HINTS ${parent_dir} NO_DEFAULT_PATH)

# The library type that is linked with `mongoc::mongoc`
set(_default_lib_type)
# Add compat targets for the mongoc-1.0 package targets
if(TARGET mongo::mongoc_shared AND NOT TARGET mongoc::shared)
    add_library(mongoc::shared IMPORTED INTERFACE)
    set_property(TARGET mongoc::shared APPEND PROPERTY INTERFACE_LINK_LIBRARIES mongo::mongoc_shared)
    set(_default_lib_type SHARED)
endif()
if(TARGET mongo::mongoc_static AND NOT TARGET mongoc::static)
    add_library(mongoc::static IMPORTED INTERFACE)
    set_property(TARGET mongoc::static APPEND PROPERTY INTERFACE_LINK_LIBRARIES mongo::mongoc_static)
    # If static is available, set it as the default library type
    set(_default_lib_type STATIC)
endif()

# Allow the user to tweak what library type is linked for `mongoc::mongoc`
set(MONGOC_DEFAULT_IMPORTED_LIBRARY_TYPE "${_default_lib_type}"
    CACHE STRING "The default library type that is used when linking against 'mongoc::mongoc' (either SHARED or STATIC, requires that the package was built with the appropriate library type)")
set_property(CACHE MONGOC_DEFAULT_IMPORTED_LIBRARY_TYPE PROPERTY STRINGS SHARED STATIC)

if(NOT TARGET mongoc::mongoc)  # Don't redefine the target if we were already included
    string(TOLOWER "${MONGOC_DEFAULT_IMPORTED_LIBRARY_TYPE}" _type)
    add_library(mongoc::mongoc IMPORTED INTERFACE)
    set_property(TARGET mongoc::mongoc APPEND PROPERTY INTERFACE_LINK_LIBRARIES mongoc::${_type})
endif()
