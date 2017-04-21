have_zlib=no
AC_MSG_CHECKING([whether to enable zlib])
AC_ARG_ENABLE([zlib],
              [AS_HELP_STRING([--enable-zlib=@<:@auto/yes/no@:>@],
                              [Enable wire protocol compression through zlib])],
              [],
              [enable_zlib=auto])
AC_MSG_RESULT([$enable_zlib])

AS_IF([test "x$enable_zlib" != "xno"],[
   PKG_CHECK_MODULES(ZLIB, [zlib], [have_zlib=yes], [
      AC_CHECK_LIB([zlib],[compress],[
         AC_CHECK_HEADER([zlib-c.h], [
            have_zlib=yes
            ZLIB_LIBS=-lz
         ], [have_zlib=no])
      ],[have_zlib=no])
   ])

   AC_MSG_CHECKING([zlib is available])
   if test "$enable_zlib" = "yes" -a "$have_zlib" = "no" ; then
      AC_MSG_ERROR([You must install the zlib development headers to enable zlib support.])
   else
      AC_MSG_RESULT([$have_zlib])
   fi
])

if test "x$have_zlib" = "xyes"; then
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_ZLIB, 1)
   enable_zlib=yes
else
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_ZLIB, 0)
   enable_zlib=no
fi
AC_SUBST(ZLIB_LIBS)
AC_SUBST(ZLIB_CFLAGS)


