if test "$with_libbson" = "auto"; then
    PKG_CHECK_MODULES(BSON, [libbson >= 0.5.0],
                      [with_libbson=system],
                      [with_libbson=bundled])
fi

if test "$with_libbson" = "system"; then
    PKG_CHECK_MODULES(BSON, [libbson >= 0.5.0])
fi

if test "$with_libbson" = "bundled"; then
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

    m4_include([src/libbson/build/autotools/CheckHeaders.m4])
    m4_include([src/libbson/build/autotools/Endian.m4])

    BSON_LT_CURRENT=lt_current
    BSON_LT_REVISION=lt_revision
    BSON_LT_AGE=lt_age
    BSON_LT_VERSION="$BSON_LT_CURRENT:$BSON_LT_REVISION:$BSON_LT_AGE"
    BSON_LT_LDFLAGS="-version-info $BSON_LT_VERSION"

    m4_include([src/libbson/build/autotools/FindDependencies.m4])

    AC_CONFIG_FILES([
        src/libbson/src/libbson-1.0.pc
        src/libbson/src/bson/bson-config.h
        src/libbson/src/bson/bson-version.h
    ])

    BSON_LIBS="libbson-1.0.la"
    BSON_CFLAGS="-I$(top_srcdir)/src/libbson/bson"

    AC_SUBST(BSON_LIBS)
    AC_SUBST(BSON_CFLAGS)
fi

