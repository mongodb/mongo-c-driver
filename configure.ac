AC_PREREQ(2.60)

m4_include([build/autotools/Versions.m4])

AC_INIT([mongo-c-driver],[mongoc_version],[https://jira.mongodb.org/browse/CDRIVER])
AC_CONFIG_MACRO_DIR([build/autotools/m4])
AC_CONFIG_AUX_DIR([build/autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I build/autotools")

MONGOC_MAJOR_VERSION=mongoc_major_version
MONGOC_MINOR_VERSION=mongoc_minor_version
MONGOC_MICRO_VERSION=mongoc_micro_version
MONGOC_API_VERSION=1.0
MONGOC_VERSION=mongoc_version
AC_SUBST(MONGOC_MAJOR_VERSION)
AC_SUBST(MONGOC_MINOR_VERSION)
AC_SUBST(MONGOC_MICRO_VERSION)
AC_SUBST(MONGOC_API_VERSION)
AC_SUBST(MONGOC_VERSION)

m4_include([build/autotools/CheckCompiler.m4])
m4_include([build/autotools/CheckHeaders.m4])
m4_include([build/autotools/CheckProgs.m4])
m4_include([build/autotools/CheckHost.m4])
m4_include([build/autotools/CheckTarget.m4])

m4_include([build/autotools/SetupLibtool.m4])
LT_INIT
MONGOC_LT_CURRENT=lt_current
MONGOC_LT_REVISION=lt_revision
MONGOC_LT_AGE=lt_age
MONGOC_LT_VERSION="$MONGOC_LT_CURRENT:$MONGOC_LT_REVISION:$MONGOC_LT_AGE"
MONGOC_LT_LDFLAGS="-version-info $MONGOC_LT_VERSION"

m4_include([build/autotools/ReadCommandLineArguments.m4])
m4_include([build/autotools/FindDependencies.m4])
m4_include([build/autotools/AutoHarden.m4])
m4_include([build/autotools/Optimizations.m4])
m4_include([build/autotools/Python.m4])

# We would put AM_INIT_AUTOMAKE into SetupAutomake.m4, but seems to cause
# autoconf errors.
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_include([build/autotools/SetupAutomake.m4])

AC_CONFIG_FILES([
    Makefile
    src/libmongoc-1.0.pc
    src/libmongoc-ssl-1.0.pc
    src/mongoc/mongoc-config.h
    src/mongoc/mongoc-version.h
])

if test "$with_libbson" = "bundled"; then
    echo "TODO: submodule init if on git"
    m4_include([src/libbson/build/autotools/Endian.m4])
    AC_CONFIG_FILES([
        src/libbson/src/bson/libbson-1.0.pc
    ])
fi

m4_include([build/autotools/PrintBuildConfiguration.m4])







dnl **************************************************************************
dnl Optionally check for OpenSSL to support TLS streams.
dnl **************************************************************************
PKG_CHECK_MODULES(SSL,  openssl, [enable_ssl=yes], [enable_ssl=no])
AM_CONDITIONAL(HAVE_SSL, test "x$enable_ssl" = "xyes")
MONGOC_ENABLE_SSL=0
if test "x$enable_ssl" = "xyes"; then
	MONGOC_ENABLE_SSL=1
fi
AC_SUBST([MONGOC_ENABLE_SSL])


dnl **************************************************************************
dnl Optionally check for Asciidoc and xmlto for documentation
dnl **************************************************************************
AC_ARG_VAR([XMLTO], [Path to xmlto command])
AC_PATH_PROG([XMLTO], [xmlto])
AC_ARG_VAR([ASCIIDOC], [Path to asciidoc command])
AC_PATH_PROG([ASCIIDOC], [asciidoc])

MONGOC_CHECK_DOC_BUILD


dnl **************************************************************************
dnl Create (3) doc targets for each public symbol
dnl **************************************************************************
MONGOC_SYMBOLS=`sed -e 's/.*/$(top_srcdir)\/doc\/&.3/' < mongoc/libmongoc.symbols | tr '\n' ' '`
AC_SUBST([MONGOC_SYMBOLS])

MONGOC_API=`sed -e 's/.*/$(top_srcdir)\/doc\/&.7/' < doc/mongoc_api | tr '\n' ' '`
AC_SUBST([MONGOC_API])

dnl **************************************************************************
dnl Check for necessary libraries.
dnl **************************************************************************
AC_CHECK_FUNCS([shm_open], [SHM_LIB=],
	       [AC_CHECK_LIB([rt], [shm_open], [SHM_LIB=-lrt],
			     [SHM_LIB=])])
AC_SUBST([SHM_LIB])

AC_CHECK_FUNCS([sched_getcpu])

AC_SEARCH_LIBS([clock_gettime], [rt], [
	AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Have clock_gettime])
])
if test "$ac_cv_search_clock_gettime" = "-lrt"; then
	LDFLAGS="$LDFLAGS -lrt"
fi


dnl **************************************************************************
dnl Enable extra debugging options
dnl **************************************************************************
m4_define([debug_default],
	  [m4_if(m4_eval(mongoc_minor_version % 2), [1],
	  [yes], [minimum])])
AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
	      		      [turn on debugging @<:@default=debug_default@:>@])],
	      		      [],
	      		      [enable_debug=debug_default])
AS_CASE([$enable_debug],
	[yes], [
		MONGOC_DEBUG_CFLAGS="-DBSON_ENABLE_DEBUG"
		MONGOC_DEBUG_CFLAGS="-DMONGOC_TRACE"
	],
	[minimum], [
		MONGOC_DEBUG_CFLAGS="-DBSON_ENABLE_DEBUG"
		MONGOC_DEBUG_CFLAGS="-DBSON_DISABLE_ASSERT"
	],
	[no], [
		MONGOC_DEBUG_CFLAGS="-DBSON_DISABLE_ASSERT"
		MONGOC_DEBUG_CFLAGS="-DBSON_DISABLE_CHECKS"
	],
	[AC_MSG_ERROR([Unknown argument to --enable-debug])]
)
AC_SUBST([MONGOC_DEBUG_CFLAGS])


dnl **************************************************************************
dnl Enable strict compiler flags
dnl **************************************************************************
# use strict compiler flags only on development releases
m4_define([maintainer_flags_default], [m4_if(m4_eval(mongoc_minor_version % 2), [1], [yes], [no])])
AC_ARG_ENABLE([maintainer-flags],
              [AS_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                              [Use strict compiler flags @<:@default=]maintainer_flags_default[@:>@])],
              [],
              [enable_maintainer_flags=maintainer_flags_default])

AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [AS_COMPILER_FLAGS([MAINTAINER_CFLAGS],
                         ["-Wall
                           -Waggregate-return
                           -Wcast-align
                           -Wdeclaration-after-statement
                           -Wempty-body
                           -Wformat
                           -Wformat-nonliteral
                           -Wformat-security
                           -Winit-self
                           -Winline
                           -Wmissing-include-dirs
                           -Wno-strict-aliasing
                           -Wno-uninitialized
                           -Wredundant-decls
                           -Wreturn-type
                           -Wshadow
                           -Wswitch-default
                           -Wswitch-enum
                           -Wundef
                           -Wuninitialized
                          "])]
)
MAINTAINER_CFLAGS="${MAINTAINER_CFLAGS#*  }"
AC_SUBST([MAINTAINER_CFLAGS])


dnl **************************************************************************
dnl Enable rdtscp instruction for fast counters
dnl **************************************************************************
AC_ARG_ENABLE([rdtscp],
	      [AS_HELP_STRING([--enable-rdtscp=@<:@no/yes@:>@],
	      		      [use rdtscp counters @<:@default=no@:>@])],
	      		      [],
	      		      [enable_rdtscp=no])
if test "x$enable_rdtscp" != "xno"; then
	CFLAGS="$CFLAGS -DENABLE_RDTSCP"
fi


dnl **************************************************************************
dnl Output
dnl **************************************************************************
AC_CONFIG_FILES([
	Makefile
	mongoc/libmongoc-1.0.pc
	mongoc/libmongoc-ssl-1.0.pc
	mongoc/mongoc-config.h
	mongoc/mongoc-version.h
])
AC_OUTPUT


if test $(( ${MONGOC_MINOR_VERSION} % 2 )) -eq 1; then
cat << EOF
 *** IMPORTANT *** 

 This is an unstable version of libmongoc.
 It is for test purposes only.

 Please, DO NOT use it in a production environment.
 It will probably crash and you will lose your data.

 Additionally, the API/ABI may change during the course
 of development.

 Thanks,

   The libmongoc team.

 *** END OF WARNING ***

EOF
fi


echo ""
echo "libmongoc-$VERSION"
echo ""
echo "  Prefix ....................: ${prefix}"
echo "  Debug Level ...............: ${enable_debug}"
echo "  Maintainer ................: ${enable_maintainer_flags}"
echo "  CFLAGS ....................: ${CFLAGS}"
echo "  SSL .......................: ${enable_ssl}"
echo "  SASL ......................: ${enable_sasl}"
echo "  Fast Counters (rdtscp).....: ${enable_rdtscp}"
echo ""
echo ""
echo "Bindings"
echo ""
echo "  Python ....................: ${ax_python_header}"
echo ""
