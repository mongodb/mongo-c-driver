dnl ##############################################################################
dnl # MONGOC_CHECK_DOC_BUILD                                                     #
dnl # Check whether to build documentation and install man-pages                 #
dnl ##############################################################################
AC_DEFUN([MONGOC_CHECK_DOC_BUILD], [{
    # Allow user to disable doc build
    AC_ARG_WITH([documentation], [AS_HELP_STRING([--without-documentation],
        [disable documentation build even if asciidoc and xmlto are present [default=no]])])

    if test "x$with_documentation" = "xno"; then
        mongoc_build_doc="no"
        mongoc_install_man="no"
    else
        # Determine whether or not documentation should be built and installed.
        mongoc_build_doc="yes"
        mongoc_install_man="yes"
        # Check for asciidoc and xmlto and don't build the docs if these are not installed.
        AC_CHECK_PROG(mongoc_have_asciidoc, asciidoc, yes, no)
        AC_CHECK_PROG(mongoc_have_xmlto, xmlto, yes, no)
        if test "x$mongoc_have_asciidoc" = "xno" -o "x$mongoc_have_xmlto" = "xno"; then
            mongoc_build_doc="no"
            # Tarballs built with 'make dist' ship with prebuilt documentation.
            if ! test -f doc/mongoc.7; then
                mongoc_install_man="no"
                AC_MSG_WARN([You are building an unreleased version of Mongoc and asciidoc or xmlto are not installed.])
                AC_MSG_WARN([Documentation will not be built and manual pages will not be installed.])
            fi
        fi

        # Do not install man pages if on mingw
        if test "x$mongoc_on_mingw32" = "xyes"; then
            mongoc_install_man="no"
        fi
    fi

    AC_MSG_CHECKING([whether to build documentation])
    AC_MSG_RESULT([$mongoc_build_doc])

    AC_MSG_CHECKING([whether to install manpages])
    AC_MSG_RESULT([$mongoc_install_man])

    AM_CONDITIONAL(BUILD_DOC, test "x$mongoc_build_doc" = "xyes")
    AM_CONDITIONAL(INSTALL_MAN, test "x$mongoc_install_man" = "xyes")
}])
