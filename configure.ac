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
m4_include([build/autotools/CheckSasl.m4])
m4_include([build/autotools/CheckSSL.m4])
m4_include([build/autotools/FindDependencies.m4])
m4_include([build/autotools/AutoHarden.m4])
m4_include([build/autotools/MaintainerFlags.m4])
m4_include([build/autotools/Optimizations.m4])
m4_include([build/autotools/LDVersionScript.m4])

# We would put AM_INIT_AUTOMAKE into SetupAutomake.m4, but seems to cause
# autoconf errors.
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_include([build/autotools/SetupAutomake.m4])

AC_CONFIG_FILES([
    Makefile
    build/version
    src/libmongoc-1.0.pc
    src/libmongoc-ssl-1.0.pc
    src/libmongoc-priv.pc
    src/mongoc/mongoc-config.h
    src/mongoc/mongoc-version.h
])

m4_include([build/autotools/Libbson.m4])

m4_include([build/autotools/PrintBuildConfiguration.m4])
