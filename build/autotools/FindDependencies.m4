
# Solaris needs to link against socket libs.
if test "$os_solaris" = "yes"; then
    CFLAGS="$CFLAGS -D__EXTENSIONS__"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE=1"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED=1"
    LDFLAGS="$LDFLAGS -lsocket -lnsl"
fi

# Check if we should enable the bundled libbson.
if "$with_libbson" = "auto"; then
   PKG_CHECK_MODULES(BSON, libbson-1.0 >= 0.5.0,
                     [with_libbson=system], [with_libbson=bundled])
fi
AM_CONDITIONAL(ENABLE_LIBBSON, [test "$with_libbson" = "bundled"])

# Check for libsasl2
PKG_CHECK_MODULES(SASL, libsasl2 >= 2.1.6, [enable_sasl=yes], [enable_sasl=no])
AM_CONDITIONAL(ENABLE_SASL, [test "$enable_sasl" = "yes"])
AC_SUBST(MONGOC_ENABLE_SASL, 0)
if test "x$enable_sasl" = "xyes"; then
    AC_SUBST(MONGOC_ENABLE_SASL, 1)
fi

# Check for openssl
PKG_CHECK_MODULES(SSL,  openssl, [enable_ssl=yes], [enable_ssl=no])
AM_CONDITIONAL(ENABLE_SSL, test "x$enable_ssl" = "xyes")
AC_SUBST(MONGOC_ENABLE_SSL, 0)
if test "x$enable_ssl" = "xyes"; then
    AC_SUBST(MONGOC_ENABLE_SSL, 1)
fi
