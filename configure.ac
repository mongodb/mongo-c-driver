dnl **************************************************************************
dnl Versioning Macros
dnl **************************************************************************

m4_define([mongoc_major_version], [0])
m4_define([mongoc_minor_version], [89])
m4_define([mongoc_micro_version], [0])
m4_define([mongoc_version], [mongoc_major_version.mongoc_minor_version.mongoc_micro_version])

# bump up by 1 for every micro release with no API changes, otherwise
# set to 0. after release, bump up by 1
m4_define([mongoc_interface_age], [0])
m4_define([mongoc_binary_age], [m4_eval(100 * mongoc_minor_version + mongoc_micro_version)])

m4_define([lt_current], [m4_eval(100 * mongoc_minor_version + mongoc_micro_version - mongoc_interface_age)])
m4_define([lt_revision], [mongoc_interface_age])
m4_define([lt_age], [m4_eval(mongoc_binary_age - mongoc_interface_age)])


dnl **************************************************************************
dnl Initialize Automake
dnl **************************************************************************
AC_PREREQ([2.63])
AC_INIT([libmongoc], [mongoc_version], [https://github.com/chergert/libmongoc/issues])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES],
	 [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])
AS_AM_REALLY_SILENT

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

AM_INIT_AUTOMAKE([1.10])
AM_PROG_CC_C_O


dnl **************************************************************************
dnl Versioning Definitions
dnl **************************************************************************
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

MONGOC_LT_CURRENT=lt_current
MONGOC_LT_REVISION=lt_revision
MONGOC_LT_AGE=lt_age
MONGOC_LT_VERSION="$MONGOC_LT_CURRENT:$MONGOC_LT_REVISION:$MONGOC_LT_AGE"
MONGOC_LT_LDFLAGS="-version-info $MONGOC_LT_VERSION"


dnl **************************************************************************
dnl Check for programs.
dnl **************************************************************************
AC_PATH_PROG([NETCAT], [nc])
AC_SUBST([NETCAT])


dnl **************************************************************************
dnl Platform Checks
dnl **************************************************************************
platform_linux=no
platform_win32=no
platform_solaris=no

AC_CANONICAL_HOST
AC_MSG_CHECKING([if building for some Win32 platform])
AS_CASE([$host],
	[*-*-mingw*|*-*-cygwin*],
	[
		MONGOC_LT_LDFLAGS="$MONGOC_LT_LDFLAGS -no-undefined"
		platform_win32=yes
	],

	[*-*-linux*],
	[
		platform_linux=yes
	],

	[*-*-solaris*],
	[
		platform_solaris=yes
		CFLAGS="$CFLAGS -D__EXTENSIONS__"
		CFLAGS="$CFLAGS -D_XOPEN_SOURCE=1"
		CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED=1"
		LDFLAGS="$LDFLAGS -lsocket"
	],

	[]
)
AC_MSG_RESULT([$platform_win32])

AM_CONDITIONAL(OS_LINUX, [test "$platform_linux" = "yes"])
AM_CONDITIONAL(OS_SOLARIS, [test "$platform_solaris" = "yes"])
AM_CONDITIONAL(OS_WIN32, [test "$platform_win32" = "yes"])

# Check for -Bsymbolic-functions to avoid intra-library PLT jumps
AC_ARG_ENABLE([Bsymbolic],
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [Avoid linking with -Bsymbolic])],
              [],
              [
                saved_LDFLAGS="${LDFLAGS}"
                AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
                LDFLAGS=-Wl,-Bsymbolic-functions
                AC_TRY_LINK([], [int main (void) { return 0; }],
                            [
                              AC_MSG_RESULT([yes])
                              enable_Bsymbolic=yes
                            ],
                            [
                              AC_MSG_RESULT([no])
                              enable_Bsymbolic=no
                            ])
                LDFLAGS="${saved_LDFLAGS}"
              ])

AS_IF([test "x$enable_Bsymbolic" = "xyes"], [MONGOC_LINK_FLAGS=-Wl[,]-Bsymbolic-functions])
MONGOC_LT_LDFLAGS="$MONGOC_LT_LDFLAGS $MONGOC_LINK_FLAGS"

AC_SUBST(MONGOC_LT_LDFLAGS)


dnl **************************************************************************
dnl Check for Required Headers
dnl **************************************************************************
AC_NEED_STDINT_H(mongoc/mongoc-stdint.h)


dnl **************************************************************************
dnl Check for Python Support
dnl **************************************************************************
AC_ARG_ENABLE(
	[python],
	[AC_HELP_STRING([--enable-python=@<:no/yes@:>@],
			[Enable support for experimental python bindings. @<:@default=no@:>@])],
	[], [
		enable_python=no
		ax_python_header=no
	]
)
AS_IF([test "x$enable_python" = "xyes"], [
	AX_PYTHON
])
AM_CONDITIONAL([HAVE_PYTHON], [test "x$ax_python_header" != "xno"])


dnl **************************************************************************
dnl Check for libbson
dnl **************************************************************************
PKG_CHECK_MODULES(BSON, libbson-1.0 >= 0.4.0)


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
MONGOC_SYMBOLS=`sed -e 's/.*/$(top_srcdir)\/doc\/&.3/' < mongoc/libmongoc.symbols | xargs`
AC_SUBST([MONGOC_SYMBOLS])

MONGOC_API=`sed -e 's/.*/$(top_srcdir)\/doc\/&.7/' < doc/mongoc_api | xargs`
AC_SUBST([MONGOC_API])

dnl **************************************************************************
dnl Check for necessary libraries.
dnl **************************************************************************
AC_CHECK_FUNCS([shm_open], [SHM_LIB=],
	       [AC_CHECK_LIB([rt], [shm_open], [SHM_LIB=-lrt],
			     [SHM_LIB=])])
AC_SUBST([SHM_LIB])


dnl **************************************************************************
dnl Check for optional components for examples.
dnl **************************************************************************
AC_CHECK_LIB([clang],
	     [clang_createIndex],
	     [enable_libclang=yes],
	     [enable_libclang=no])
AM_CONDITIONAL(HAVE_CLANG, test "x$enable_libclang" = "xyes")


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
                           -Wunused-but-set-parameter
                           -Wuninitialized
                          "])]
)
MAINTAINER_CFLAGS="${MAINTAINER_CFLAGS#*  }"
AC_SUBST([MAINTAINER_CFLAGS])


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
echo ""
echo "Examples"
echo ""
echo "  index-source ..............: ${enable_libclang}"
echo "  mongoc-ping ...............: yes"
echo ""
echo "Bindings"
echo ""
echo "  Python ....................: ${ax_python_header}"
echo ""
