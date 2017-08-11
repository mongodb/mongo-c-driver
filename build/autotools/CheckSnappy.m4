# If --with-snappy=auto, determine if there is a system installed snappy
# greater than our required version.
AS_IF([test "x${with_snappy}" = xauto], [
      PKG_CHECK_MODULES(SNAPPY, [snappy],
         [with_snappy=system],
         [
            # If we didn't find snappy with pkgconfig
            with_snappy=no
            AC_CHECK_LIB([snappy],[snappy_uncompress],
               [AC_CHECK_HEADER([snappy-c.h],
                  [
                     with_snappy=system
                     SNAPPY_LIBS=-lsnappy
                  ],
                  []
               )],
               []
            )
         ]
      )
   ]
)

AS_IF([test "x${SNAPPY_LIBS}" = "x" -a "x$with_snappy" = "xsystem"],
      [AC_MSG_ERROR([Cannot find system installed snappy. try --with-snappy=bundled])])

# If we are using the bundled snappy, recurse into its configure.
AS_IF([test "x${with_snappy}" = xbundled],[
   AC_MSG_CHECKING(whether to enable bundled snappy)
   AC_ERROR(bundled snappy is not currently supported)
])

if test "x$with_snappy" != "xno"; then
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 1)
else
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_SNAPPY, 0)
fi
AC_SUBST(SNAPPY_LIBS)
AC_SUBST(SNAPPY_CFLAGS)

