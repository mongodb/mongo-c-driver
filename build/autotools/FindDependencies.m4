
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
AC_SUBST(MONGOC_ENABLE_SASL, 0)
AS_IF([test "$enable_sasl" = "auto"],
      [PKG_CHECK_MODULES(SASL, libsasl2 >= sasl_required_version,
                         [enable_sasl=yes], [enable_sasl=no])])
AS_IF([test "$enable_sasl" = "yes"],
      [PKG_CHECK_MODULES(SASL, libsasl2 >= sasl_required_version)
       AC_SUBST(MONGOC_ENABLE_SASL, 1)])
AM_CONDITIONAL(ENABLE_SASL, test "$enable_sasl" = "yes")

# Check for openssl
AC_SUBST(MONGOC_ENABLE_SSL, 0)
AS_IF([test "$enable_ssl" = "auto"],
      [PKG_CHECK_MODULES(SSL, openssl, [enable_ssl=yes], [enable_ssl=no])])
AS_IF([test "$enable_ssl" = "yes"],
      [PKG_CHECK_MODULES(SSL, openssl)
       AC_SUBST(MONGOC_ENABLE_SSL, 1)])
AM_CONDITIONAL(ENABLE_SSL, test "$enable_ssl" = "yes")

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
AS_IF([test "$ac_cv_search_clock_gettime" = "-lrt"],
      [LDFLAGS="$LDFLAGS -lrt"])

AS_IF([test "$enable_rdtscp" = "yes"],
      [CPPFLAGS="$CPPFLAGS -DENABLE_RDTSCP"])
