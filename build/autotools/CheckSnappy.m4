# If --with-snappy=auto, determine if there is a system installed snappy
# greater than our required version.
found_snappy=no

AS_IF([test "x${with_snappy}" = xauto -o "x${with_snappy}" = xsystem], [
   PKG_CHECK_MODULES(SNAPPY, [snappy], [
      found_snappy=yes
   ], [
      # If we didn't find snappy with pkgconfig, search manually. If that
      # fails and with-snappy=system, fail, or if with-snappy=auto, use
      # bundled.
      AC_CHECK_LIB([snappy], [snappy_uncompress], [
         AC_CHECK_HEADER([snappy-c.h], [
            found_snappy=yes
         ])
      ])
   ])
])

AS_IF([test "x${found_snappy}" = xyes], [
   with_snappy=system
   SNAPPY_LIBS=-lsnappy
], [
   # snappy not found
   AS_IF([test "x${with_snappy}" = xauto -o "x${with_snappy}" = xbundled], [
      with_snappy=bundled
   ], [
      # snappy not found, with-snappy=system
      AC_MSG_ERROR([Cannot find system installed snappy. try --with-snappy=bundled])
   ]
)])

# If we are using the bundled snappy, recurse into its configure.
AS_IF([test "x${with_snappy}" = xbundled],[
   AC_MSG_CHECKING(whether to enable bundled snappy)
   AC_MSG_RESULT(yes)

   # start of vendored configure.ac checks from snappy
   AC_CHECK_HEADERS([stdint.h stddef.h sys/mman.h sys/resource.h sys/uio.h windows.h byteswap.h sys/byteswap.h sys/endian.h sys/time.h])
   # See if we have __builtin_expect.
   AC_MSG_CHECKING([if the compiler supports __builtin_expect])

   AC_TRY_COMPILE(, [
       return __builtin_expect(1, 1) ? 1 : 0
   ], [
       snappy_have_builtin_expect=yes
       AC_MSG_RESULT([yes])
   ], [
       snappy_have_builtin_expect=no
       AC_MSG_RESULT([no])
   ])
   if test x$snappy_have_builtin_expect = xyes ; then
       AC_DEFINE([HAVE_BUILTIN_EXPECT], [1], [Define to 1 if the compiler supports __builtin_expect.])
   fi

   # See if we have working count-trailing-zeros intrinsics.
   AC_MSG_CHECKING([if the compiler supports __builtin_ctzll])

   AC_TRY_COMPILE(, [
       return (__builtin_ctzll(0x100000000LL) == 32) ? 1 : 0
   ], [
       snappy_have_builtin_ctz=yes
       AC_MSG_RESULT([yes])
   ], [
       snappy_have_builtin_ctz=no
       AC_MSG_RESULT([no])
   ])
   if test x$snappy_have_builtin_ctz = xyes ; then
       AC_DEFINE([HAVE_BUILTIN_CTZ], [1], [Define to 1 if the compiler supports __builtin_ctz and friends.])
   fi
   # These are used by snappy-stubs-public.h.in.
   if test "$ac_cv_header_stdint_h" = "yes"; then
       AC_SUBST([ac_cv_have_stdint_h], [1])
   else
       AC_SUBST([ac_cv_have_stdint_h], [0])
   fi
   if test "$ac_cv_header_stddef_h" = "yes"; then
       AC_SUBST([ac_cv_have_stddef_h], [1])
   else
       AC_SUBST([ac_cv_have_stddef_h], [0])
   fi
   if test "$ac_cv_header_sys_uio_h" = "yes"; then
       AC_SUBST([ac_cv_have_sys_uio_h], [1])
   else
       AC_SUBST([ac_cv_have_sys_uio_h], [0])
   fi
   # end of vendored configure.ac checks from snappy

   SNAPPY_LIBS=
   SNAPPY_CFLAGS="-Isrc/snappy-1.1.3"
])

if test "x${with_snappy}" != "xno"; then
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 1)
else
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 0)
fi
AC_SUBST(SNAPPY_LIBS)
AC_SUBST(SNAPPY_CFLAGS)

