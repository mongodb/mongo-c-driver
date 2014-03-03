# If --with-libbson=auto, determine if there is a system installed libbson
# greater than our required version.
AS_IF([test "$with_libbson" = "auto"],
      [PKG_CHECK_MODULES(BSON, [libbson >= libbson_required_version],
                         [with_libbson=system],
                         [with_libbson=bundled])])

# If we are to use the system, check for libbson enforcing success.
AS_IF([test "$with_libbson" = "system"],
      [PKG_CHECK_MODULES(BSON, [libbson >= libbson_required_version])])

# If we are using the bundled libbson, recurse into its configure.
AS_IF([test "$with_libbson" = "bundled"],
      [AC_CONFIG_SUBDIRS([src/libbson])
       AC_DEFINE(BSON_CFLAGS, [-Isrc/libbson/src/bson])
       AC_DEFINE(BSON_LIBS, [libbson-1.0.la])
       AC_SUBST(BSON_CFLAGS)
       AC_SUBST(BSON_LIBS)])
