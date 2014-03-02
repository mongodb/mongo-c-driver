AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
    AC_MSG_ERROR([You need 'perl' to compile libbson])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
    AC_MSG_ERROR([You need 'mv' to compile libbson])
fi

AC_PATH_PROG(GREP, grep)
if test -z "$GREP"; then
    AC_MSG_ERROR([You need 'grep' to compile libbson])
fi

AC_PROG_INSTALL

AC_ARG_VAR([XMLTO], [Path to xmlto command])
AC_PATH_PROG([XMLTO], [xmlto])
AC_ARG_VAR([ASCIIDOC], [Path to asciidoc command])
AC_PATH_PROG([ASCIIDOC], [asciidoc])

MONGOC_CHECK_DOC_BUILD
MONGOC_SYMBOLS=`sed -e 's/.*/$(top_srcdir)\/doc\/&.3/' < src/libmongoc.symbols | tr '\n' ' '`
AC_SUBST([MONGOC_SYMBOLS])

MONGOC_API=`sed -e 's/.*/$(top_srcdir)\/doc\/&.7/' < doc/mongoc_api | tr '\n' ' '`
AC_SUBST([MONGOC_API])
