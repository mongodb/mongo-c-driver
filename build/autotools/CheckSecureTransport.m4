AC_MSG_CHECKING([whether to use native Secure Transport on OSX/iOS])
AC_ARG_ENABLE([secure-transport],
              [AC_HELP_STRING([--enable-secure-transport],
                              [Use native TLS on OSX/iOS [default=no]])],
              [],
              [enable_secure_transport="no"])
AC_MSG_RESULT([$enable_secure_transport])

AS_IF([test "$enable_secure_transport" != "no"],[
  dnl Link against the Secure Transport frameworks
  if test "$enable_secure_transport" = "yes" ; then
    SSL_LIBS="-framework Security -framework CoreFoundation"
  fi
])

AS_IF([test "$os_darwin" != "yes" -a "$enable_secure_transport" = "yes" ],
   [AC_MSG_ERROR([cannot build against Secure Transport on non-darwin platform])]
)
AM_CONDITIONAL([ENABLE_SECURE_TRANSPORT], [test "$enable_secure_transport" = "yes"])
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)

if test "$enable_secure_transport" = "yes" ; then
  AC_SUBST(MONGOC_ENABLE_SECURE_TRANSPORT, 1)
else
  AC_SUBST(MONGOC_ENABLE_SECURE_TRANSPORT, 0)
fi
