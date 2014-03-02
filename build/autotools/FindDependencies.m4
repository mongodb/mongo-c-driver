
# Solaris needs to link against socket libs.
if test "$os_solaris" = "yes"; then
    CFLAGS="$CFLAGS -D__EXTENSIONS__"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE=1"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED=1"
    LDFLAGS="$LDFLAGS -lsocket -lnsl"
fi

# Check if we should enable the bundled libbson.
if test "$with_libbson" = "auto"; then
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

# Check for shm functions.
AC_CHECK_FUNCS([shm_open], [SHM_LIB=],
               [AC_CHECK_LIB([rt], [shm_open], [SHM_LIB=-lrt], [SHM_LIB=])])
AC_SUBST([SHM_LIB])

# Check for sched_getcpu
AC_CHECK_FUNCS([sched_getcpu])

# Check for clock_gettime
AC_SEARCH_LIBS([clock_gettime], [rt], [
    AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Have clock_gettime])
])
if test "$ac_cv_search_clock_gettime" = "-lrt"; then
    LDFLAGS="$LDFLAGS -lrt"
fi

if test "x$enable_rdtscp" != "xno"; then
	CPPFLAGS="$CPPFLAGS -DENABLE_RDTSCP"
fi
