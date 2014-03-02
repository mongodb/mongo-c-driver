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
                           -Wuninitialized
                          "])]
)
MAINTAINER_CFLAGS="${MAINTAINER_CFLAGS#*  }"
AC_SUBST([MAINTAINER_CFLAGS])
