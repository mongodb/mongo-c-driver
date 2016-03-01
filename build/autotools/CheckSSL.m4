AC_MSG_CHECKING([whether to enable crypto and TLS])
AC_ARG_ENABLE([ssl],
              [AS_HELP_STRING([--enable-ssl=@<:@auto/no/openssl/native@:>@],
                              [Enable TLS connections and SCRAM-SHA-1 authentication.])],
              [],
              [enable_ssl=auto])
AC_MSG_RESULT([$enable_ssl])

AS_IF([test "$enable_ssl" != "no"],[
   AS_IF([test "$enable_ssl" != "native"],[
      PKG_CHECK_MODULES(SSL, [openssl], [enable_openssl=auto], [
         AC_CHECK_LIB([ssl],[SSL_library_init],[have_ssl_lib=yes],[have_ssl_lib=no])
         AC_CHECK_LIB([crypto],[CRYPTO_set_locking_callback],[have_crypto_lib=yes],[have_crypto_lib=no])

         if test "$have_ssl_lib" = "no" -o "$have_crypto_lib" = "no" ; then
            if test "$enable_ssl" = "openssl"; then
               AC_MSG_ERROR([You must install the OpenSSL libraries and development headers to enable OpenSSL support.])
            else
               AC_MSG_WARN([You must install the OpenSSL libraries and development headers to enable OpenSSL support.])
            fi
         fi

         if test "$have_ssl_lib" = "yes" -a "$have_crypto_lib" = "yes"; then
            SSL_LIBS="-lssl -lcrypto"
            enable_ssl=openssl
         fi
      ])
   ])
   dnl PKG_CHECK_MODULES() doesn't check for headers
   dnl OSX for example has the lib, but not headers, so double confirm if OpenSSL works
   AS_IF([test "$enable_ssl" = "openssl" -o "$enable_openssl" = "auto"], [
      AC_CHECK_HEADERS([openssl/bio.h openssl/ssl.h openssl/err.h openssl/crypto.h],
         [have_ssl_headers=yes],
         [have_ssl_headers=no])
      if test "$have_ssl_headers" = "yes"; then
         enable_ssl=openssl
      elif test "$enable_ssl" = "openssl"; then
         AC_MSG_ERROR([You must install the OpenSSL development headers to enable OpenSSL support.])
      fi
   ])
   AS_IF([test "$enable_ssl" != "openssl" -a "$os_darwin" = "yes"],[
      SSL_LIBS="-framework Security -framework CoreFoundation"
      enable_ssl="darwin"
   ])
   AC_MSG_CHECKING([which TLS library to use])
   AC_MSG_RESULT([$enable_ssl])
], [enable_ssl=no])




AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LIBS)


if test "$enable_ssl" = "darwin" -o "$enable_ssl" = "openssl"; then
   AC_SUBST(MONGOC_ENABLE_SSL,     1)
   AC_SUBST(MONGOC_ENABLE_CRYPTO,  1)
   if test "$enable_ssl" = "darwin"; then
      AC_SUBST(MONGOC_ENABLE_OPENSSL,           0)
      AC_SUBST(MONGOC_ENABLE_LIBCRYPTO,         0)
      AC_SUBST(MONGOC_ENABLE_COMMON_CRYPTO,     1)
      AC_SUBST(MONGOC_ENABLE_SECURE_TRANSPORT,  1)
   elif test "$enable_ssl" = "openssl"; then
      AC_SUBST(MONGOC_ENABLE_OPENSSL,           1)
      AC_SUBST(MONGOC_ENABLE_LIBCRYPTO,         1)
      AC_SUBST(MONGOC_ENABLE_COMMON_CRYPTO,     0)
      AC_SUBST(MONGOC_ENABLE_SECURE_TRANSPORT,  0)
   fi
else
   AC_SUBST(MONGOC_ENABLE_SSL,               0)
   AC_SUBST(MONGOC_ENABLE_CRYPTO,            0)
   AC_SUBST(MONGOC_ENABLE_OPENSSL,           0)
   AC_SUBST(MONGOC_ENABLE_LIBCRYPTO,         0)
   AC_SUBST(MONGOC_ENABLE_COMMON_CRYPTO,     0)
   AC_SUBST(MONGOC_ENABLE_SECURE_TRANSPORT,  0)
fi

AM_CONDITIONAL([ENABLE_SSL],              [test "$enable_ssl" = "darwin" -o "$enable_ssl" = "openssl"])
AM_CONDITIONAL([ENABLE_CRYPTO],           [test "$enable_ssl" = "darwin" -o "$enable_ssl" = "openssl"])
AM_CONDITIONAL([ENABLE_OPENSSL],          [test "$enable_ssl" = "openssl"])
AM_CONDITIONAL([ENABLE_LIBCRYPTO],        [test "$enable_ssl" = "openssl"])
AM_CONDITIONAL([ENABLE_COMMON_CRYPTO],    [test "$enable_ssl" = "darwin"])
AM_CONDITIONAL([ENABLE_SECURE_TRANSPORT], [test "$enable_ssl" = "darwin"])

