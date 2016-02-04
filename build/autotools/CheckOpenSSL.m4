AC_ARG_ENABLE([openssl],
              [AS_HELP_STRING([--enable-openssl=@<:@auto/yes/no@:>@],
                              [Use OpenSSL for TLS connections and SCRAM-SHA-1 authentication.])],
              [],
              [enable_openssl=auto])

AS_IF([test "$enable_openssl" != "no"],[
  PKG_CHECK_MODULES(SSL, [openssl], [enable_openssl=yes], [
    AS_IF([test "$enable_openssl" != "no"],[
      AC_CHECK_LIB([ssl],[SSL_library_init],[have_ssl_lib=yes],[have_ssl_lib=no])
      AC_CHECK_LIB([crypto],[CRYPTO_set_locking_callback],[have_crypto_lib=yes],[have_crypto_lib=no])
      if test "$enable_openssl" = "yes"; then
        if test "$have_ssl_lib" = "no" -o "$have_crypto_lib" = "no" ; then
          AC_MSG_ERROR([You must install the OpenSSL libraries and development headers to enable OpenSSL support.])
        fi
      fi

      AC_CHECK_HEADERS([openssl/bio.h openssl/ssl.h openssl/err.h openssl/crypto.h],
                       [have_ssl_headers=yes],
                       [have_ssl_headers=no])
      if test "$have_ssl_headers" = "no" -a "$enable_openssl" = "yes" ; then
        AC_MSG_ERROR([You must install the OpenSSL development headers to enable OpenSSL support.])
      fi

      if test "$have_ssl_headers" = "yes" -a "$have_ssl_lib" = "yes" -a "$have_crypto_lib" = "yes"; then
        SSL_LIBS="-lssl -lcrypto"
        enable_openssl=yes
      else
        enable_openssl=no
      fi
    ])
  ])
])


AM_CONDITIONAL([ENABLE_OPENSSL], [test "$enable_openssl" = "yes"])
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)

dnl Let mongoc-config.h.in know about SSL status.
if test "$enable_openssl" = "yes" ; then
  AC_SUBST(MONGOC_ENABLE_OPENSSL, 1)
else
  AC_SUBST(MONGOC_ENABLE_OPENSSL, 0)
fi
