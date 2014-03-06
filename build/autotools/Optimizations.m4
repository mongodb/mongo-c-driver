OPTIMIZE_CFLAGS=""
OPTIMIZE_LDFLAGS=""

# Check if we should use -Bsymbolic
AS_IF([test "$enable_optimizations" != "no"], [
    check_link_flag([-Wl,-Bsymbolic], [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -Wl,-Bsymbolic"])
])

# Check if we should use link-time-optimization.
#AS_IF([test "$enable_optimizations" != "no"], [
#    check_cc_cxx_flag([-flto], [OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS -flto"])
#    check_link_flag([-flto], [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -flto"])
#])

AC_SUBST(OPTIMIZE_CFLAGS)
AC_SUBST(OPTIMIZE_LDFLAGS)


# Add '-g' flag to gcc to build with debug symbols.
if test "$enable_debug_symbols" = "min"; then
    CFLAGS="$CFLAGS -g1"
elif test "$enable_debug_symbols" != "no"; then
    CFLAGS="$CFLAGS -g"
fi

# Add the appropriate 'O' level for optimized builds.
if test "$enable_optimizations" = "yes"; then
    CFLAGS="$CFLAGS -O2"

    if test "$c_compiler" = "gcc"; then
        CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
    fi
else
    CFLAGS="$CFLAGS -O0"
fi

AS_IF([test $"enable_debug" = "no"],
      [CPPFLAGS="$CPPFLAGS -DBSON_DISABLE_ASSERT"
       CPPFLAGS="$CPPFLAGS -DBSON_DISABLE_CHECKS"])

AS_IF([test "$enable_tracing" = "yes"],
      [CPPFLAGS="$CPPFLAGS -DMONGOC_TRACE"])
