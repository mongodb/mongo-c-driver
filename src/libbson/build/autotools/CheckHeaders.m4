AC_HEADER_STDBOOL
AC_SUBST(BSON_HAVE_STDBOOL_H, 0)
if test "$ac_cv_header_stdbool_h" = "yes"; then
	AC_SUBST(BSON_HAVE_STDBOOL_H, 1)
fi

AC_CREATE_STDINT_H([src/bson/bson-stdint.h])

# Determine value of obsolete HAVE_STRINGS_H.
AC_CHECK_HEADERS_ONCE([strings.h])

# Temporary while we use both Autotools and CMake: set BSON_HAVE_STRINGS_H.
AC_SUBST(BSON_HAVE_STRINGS_H, 0)
AC_CHECK_HEADERS_ONCE([strings.h],
    [bson_cv_have_strings_h=no],
    [bson_cv_have_strings_h=yes])

if test "$bson_cv_have_strings_h" = yes; then
    AC_SUBST(BSON_HAVE_STRINGS_H, 1)
fi
