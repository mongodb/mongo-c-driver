# Initialize the submodule if necessary.
if test -d .git; then
    git submodule init
    git submodule update
fi

m4_include([src/libbson/build/autotools/Versions.m4])

BSON_MAJOR_VERSION=bson_major_version
BSON_MINOR_VERSION=bson_minor_version
BSON_MICRO_VERSION=bson_micro_version
BSON_API_VERSION=1.0
BSON_VERSION=bson_version
AC_SUBST(BSON_MAJOR_VERSION)
AC_SUBST(BSON_MINOR_VERSION)
AC_SUBST(BSON_MICRO_VERSION)
AC_SUBST(BSON_API_VERSION)
AC_SUBST(BSON_VERSION)

m4_include([build/autotools/CheckHeaders.m4])
m4_include([src/libbson/build/autotools/Endian.m4])

BSON_LT_CURRENT=lt_current
BSON_LT_REVISION=lt_revision
BSON_LT_AGE=lt_age
BSON_LT_VERSION="$BSON_LT_CURRENT:$BSON_LT_REVISION:$BSON_LT_AGE"
BSON_LT_LDFLAGS="-version-info $BSON_LT_VERSION"

m4_include([build/autotools/FindDependencies.m4])

AC_CONFIG_FILES([
    src/libbson/src/libbson-1.0.pc
    src/libbson/src/bson/bson-config.h
    src/libbson/src/bson/bson-version.h
])
