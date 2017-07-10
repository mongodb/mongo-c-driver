# If --with-zlib=auto, determine if there is a system installed zlib
AS_IF([test "x${with_zlib}" = xauto], [
      PKG_CHECK_MODULES(ZLIB, [zlib],
         [with_zlib=system],
         [
            # If we didn't find zlib with pkgconfig
            with_zlib=no
            AC_CHECK_LIB([zlib],[compress2],
               [AC_CHECK_HEADER([zlib-c.h],
                  [
                     with_zlib=system
                     ZLIB_LIBS=-lz
                  ],
                  []
               )],
               []
            )
         ]
      )
   ]
)

AS_IF([test "x${ZLIB_LIBS}" = "x" -a "x$with_zlib" = "xsystem"],
      [AC_MSG_ERROR([Cannot find system installed zlib. try --with-zlib=bundled])])

# If we are using the bundled zlib, recurse into its configure.
AS_IF([test "x${with_zlib}" = xbundled],[
   AC_MSG_CHECKING(whether to enable bundled zlib)
   AC_ERROR(bundled zlib is not currently supported)
])

if test "x$with_zlib" != "xno"; then
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_ZLIB, 1)
else
   AC_SUBST(MONGOC_ENABLE_COMPRESSION_ZLIB, 0)
fi
AC_SUBST(ZLIB_LIBS)
AC_SUBST(ZLIB_CFLAGS)

