# If CFLAGS and CXXFLAGS are unset, default to empty.
# This is to tell automake not to include '-g' if C{XX,}FLAGS is not set.
# For more info - http://www.gnu.org/software/automake/manual/autoconf.html#C_002b_002b-Compiler
if test -z "$CXXFLAGS"; then
    CXXFLAGS=""
fi
if test -z "$CFLAGS"; then
    CFLAGS=""
fi

AC_PROG_CC
AC_PROG_CXX

# Check that an appropriate C compiler is available.
c_compiler="unknown"
AC_LANG_PUSH([C])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 1)
#error Not a supported GCC compiler
#endif
])], [c_compiler="gcc"], [])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 3)))
#error Not a supported Clang compiler
#endif
])], [c_compiler="clang"], [])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__SUNPRO_C))
#error Not a supported Sun compiler
#endif
])], [c_compiler="sun"], [])
AC_LANG_POP([C])

if test "$c_compiler" = "unknown"; then
    AC_MSG_ERROR([Compiler GCC >= 4.1 or Clang >= 3.3 is required for C compilation])
fi

# GLibc 2.19 complains about both _BSD_SOURCE and _GNU_SOURCE. The _GNU_SOURCE
# contains everything anyway. So just use that.
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <features.h>
#ifndef __GLIBC__
#error not glibc
#endif
]], [])],
LIBC_FEATURES="-D_GNU_SOURCE",
LIBC_FEATURES="-D_BSD_SOURCE")
AC_SUBST(LIBC_FEATURES)

AC_C_CONST
AC_C_INLINE
AC_C_TYPEOF
