find_package(PkgConfig)
pkg_check_modules(PC_utf8proc QUIET utf8proc)

find_path(utf8proc_INCLUDE_DIR
    NAMES utf8proc.h
    PATHS ${PC_utf8proc_INCLUDE_DIRS}
    PATH_SUFFIXES utf8proc-2.8.0
)

set(utf8proc_VERSION ${PC_utf8proc_VERSION})

mark_as_advanced(utf8proc_FOUND utf8proc_INCLUDE_DIR utf8proc_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(utf8proc
    REQUIRED_VARS utf8proc_INCLUDE_DIR
    VERSION_VAR utf8proc_VERSION
)