have_snappy=no
AC_MSG_CHECKING([whether to enable snappy])
AC_ARG_ENABLE([snappy],
              [AS_HELP_STRING([--enable-snappy=@<:@auto/yes/no@:>@],
                              [Enable wire protocol compression through snappy])],
              [],
              [enable_snappy=auto])
AC_MSG_RESULT([$enable_snappy])

AS_IF([test "x$enable_snappy" != "xno"],[
   PKG_CHECK_MODULES(SNAPPY, [snappy], [have_snappy=yes], [
      AC_CHECK_LIB([snappy],[snappy_uncompress],[
         AC_CHECK_HEADER([snappy-c.h], [
            have_snappy=yes
            SNAPPY_LIBS=-lsnappy
         ], [have_snappy=no])
      ],[have_snappy=no])
   ])

   AC_MSG_CHECKING([snappy is available])
   if test "$enable_snappy" = "yes" -a "$have_snappy" = "no" ; then
      AC_MSG_ERROR([You must install the snappy development headers to enable snappy support.])
   else
      AC_MSG_RESULT([$have_snappy])
   fi
])

if test "x$have_snappy" = "xyes"; then
   AC_SUBST(MONGOC_ENABLE_COMPRESSION, 1)
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 1)
   enable_snappy=yes
else
   AC_SUBST(MONGOC_ENABLE_COMPRESSION, 0)
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 0)
   enable_snappy=no
fi
AC_SUBST(SNAPPY_LIBS)
AC_SUBST(SNAPPY_CFLAGS)


