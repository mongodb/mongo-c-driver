AC_DEFUN([mongoc_parse_version], [
    m4_define([ver_split], m4_split(m4_translit($2, [-], [.]), [\.]))
    m4_define([$1_major_version], m4_argn(1, ver_split))
    m4_define([$1_minor_version], m4_argn(2, ver_split))
    m4_define([$1_micro_version], m4_argn(3, ver_split))
    m4_define([$1_prerelease_version], m4_argn(4, ver_split))

    # Set mongoc_version / mongoc_released_version to "x.y.z" from components.
    m4_define(
        [$1_version],
        m4_ifset(
            [$1_prerelease_version],
            [$1_major_version.$1_minor_version.$1_micro_version-$1_prerelease_version],
            [$1_major_version.$1_minor_version.$1_micro_version]))

    # E.g., if prefix is "mongoc", substitute MONGOC_MAJOR_VERSION.
    m4_define([prefix_upper], [translit([$1],[a-z],[A-Z])])
    AC_SUBST(m4_join([], prefix_upper, [_MAJOR_VERSION]), $1_major_version)
    AC_SUBST(m4_join([], prefix_upper, [_MINOR_VERSION]), $1_minor_version)
    AC_SUBST(m4_join([], prefix_upper, [_MICRO_VERSION]), $1_micro_version)
    AC_SUBST(m4_join([], prefix_upper, [_PRERELEASE_VERSION]), $1_prerelease_version)

    # Substitute the joined version string, e.g. set MONGOC_VERSION to $mongoc_version.
    AC_SUBST(m4_join([], prefix_upper, [_VERSION]), $1_version)
])

# Parse version, perhaps like "x.y.z-dev", from VERSION_CURRENT file.
mongoc_parse_version(mongoc, m4_esyscmd_s(cat VERSION_CURRENT))

# Parse most recent stable release, like "x.y.z", from VERSION_RELEASED file.
mongoc_parse_version(mongoc_released, m4_esyscmd_s(cat VERSION_RELEASED))

AC_MSG_NOTICE([Current version (from VERSION_CURRENT file): $MONGOC_VERSION])

m4_ifset([mongoc_released_prerelease_version],
         [AC_ERROR([RELEASED_VERSION file has prerelease version $MONGOC_RELEASED_VERSION])])

if test "x$mongoc_version" != "x$mongoc_released_version"; then
    AC_MSG_NOTICE([Most recent release (from VERSION_RELEASED file): $MONGOC_RELEASED_VERSION])
    m4_ifset([mongoc_prerelease_version], [], [
        AC_ERROR([Current version must be a prerelease (with "-dev", "-beta", etc.) or equal to previous release])
    ])
fi

# bump up by 1 for every micro release with no API changes, otherwise
# set to 0. after release, bump up by 1
m4_define([mongoc_released_interface_age], mongoc_released_micro_version)
m4_define([mongoc_released_binary_age],
          [m4_eval(100 * mongoc_released_minor_version +
                   mongoc_released_micro_version)])

AC_SUBST([MONGOC_RELEASED_INTERFACE_AGE], [mongoc_released_interface_age])
AC_MSG_NOTICE([libmongoc interface age $MONGOC_RELEASED_INTERFACE_AGE])

m4_define([lt_current],
          [m4_eval(100 * mongoc_released_minor_version +
                   mongoc_released_micro_version -
                   mongoc_released_interface_age)])

m4_define([lt_revision], [mongoc_released_interface_age])

m4_define([lt_age], [m4_eval(mongoc_released_binary_age -
                     mongoc_released_interface_age)])

# So far, we've synchronized libbson and mongoc versions.
m4_define([libbson_required_version], [mongoc_released_version])

m4_define([sasl_required_version], [2.1.6])
