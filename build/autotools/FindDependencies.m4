
# Solaris needs to link against socket libs.
if test "$os_solaris" = "yes"; then
    CFLAGS="$CFLAGS -D__EXTENSIONS__"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE=1"
    CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED=1"
    LDFLAGS="$LDFLAGS -lsocket -lnsl"
fi

# Check if we should enable the bundled libbson.
if test "$with_libbson" = "auto"; then
   PKG_CHECK_MODULES(BSON, libbson-1.0 >= libbson_required_version,
                     [with_libbson=system], [with_libbson=bundled])
fi
AM_CONDITIONAL(ENABLE_LIBBSON, [test "$with_libbson" = "bundled"])

# Check for shm functions.
AC_CHECK_FUNCS([shm_open], [SHM_LIB=],
               [AC_CHECK_LIB([rt], [shm_open], [SHM_LIB=-lrt], [SHM_LIB=])])
AC_SUBST([SHM_LIB])

# Check for sched_getcpu
AC_CHECK_FUNCS([sched_getcpu],
               [AC_SUBST(MONGOC_HAVE_SCHED_GETCPU, 1)],
               [AC_SUBST(MONGOC_HAVE_SCHED_GETCPU, 0)])

AS_IF([test "enable_rdtscp" = "yes"], [
   AC_SUBST(MONGOC_ENABLE_RDTSCP, 1)
], [
   AC_SUBST(MONGOC_ENABLE_RDTSCP, 0)
])

AS_IF([test "$enable_shm_counters" = "yes"],
      [AC_SUBST(MONGOC_ENABLE_SHM_COUNTERS, 1)],
      [AC_SUBST(MONGOC_ENABLE_SHM_COUNTERS, 0)])

AC_CHECK_TYPE([socklen_t],
              [AC_SUBST(MONGOC_HAVE_SOCKLEN, 1)],
              [AC_SUBST(MONGOC_HAVE_SOCKLEN, 0)],
              [#include <sys/socket.h>])

AX_PTHREAD
