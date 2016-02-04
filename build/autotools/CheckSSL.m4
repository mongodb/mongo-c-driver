AC_ARG_ENABLE([ssl],
              [AS_HELP_STRING([--enable-ssl=@<:@auto/yes/no@:>@],
                              [Deprecated. Please use --enable-openssl to use OpenSSL. Default: auto])],
              [],
              [enable_ssl=auto])

AS_IF([test "$enable_openssl" != "no"],[
if test "$enable_openssl" = "yes"; then
    enable_ssl="yes";
else
    enable_ssl="no";
fi])

AM_CONDITIONAL([ENABLE_SSL], [test "$enable_ssl" = "yes"])

dnl Let mongoc-config.h.in know about SSL status.
if test "$enable_ssl" = "yes" ; then
  AC_SUBST(MONGOC_ENABLE_SSL, 1)
else
  AC_SUBST(MONGOC_ENABLE_SSL, 0)
fi
