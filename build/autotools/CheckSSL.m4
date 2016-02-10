AC_MSG_CHECKING([whether to enable TLS])
AC_ARG_ENABLE([ssl],
              [AS_HELP_STRING([--enable-ssl=@<:@yes/no@:>@],
                              [Deprecated. Please use --enable-openssl to use OpenSSL. Default: auto])],
              [],
              [enable_ssl=no])

AS_IF([test "$enable_openssl" != "no"],[
if test "$enable_openssl" = "yes"; then
    enable_ssl="openssl";
fi])
AS_IF([test "$enable_secure_transport" != "no"],[
if test "$enable_secure_transport" = "yes"; then
    enable_ssl="secure_transport";
fi])

AM_CONDITIONAL([ENABLE_SSL], [test "$enable_ssl" != "no"])

AS_IF([test "$enable_openssl" != "yes" -a "$enable_secure_transport" = "yes" ],
   [AC_MSG_ERROR([cannot build against both OpenSSL and Secure Transport])]
)

dnl Let mongoc-config.h.in know about SSL status.
if test "$enable_ssl" != "no" ; then
  AC_SUBST(MONGOC_ENABLE_SSL, 1)
else
  AC_SUBST(MONGOC_ENABLE_SSL, 0)
fi
AC_MSG_RESULT([$enable_ssl])
