#include "TestSuite.h"

#include <mlib/intutil.h>
#include <mlib/config.h>
#include <mlib/cmp.h>

#include <stddef.h>

mlib_diagnostic_push (); // We don't set any diagnostics, we just want to make sure it compiles

// Not relevant, we just want to test that it compiles:
mlib_msvc_warning (disable : 4507);

static void
_test_minmax (void)
{
   mlib_static_assert (mlib_minof (unsigned) == 0);
   // Ambiguous signedness, still works:
   mlib_static_assert (mlib_minof (char) == CHAR_MIN);
   mlib_static_assert (mlib_maxof (char) == CHAR_MAX);

   mlib_static_assert (mlib_minof (uint8_t) == 0);
   mlib_static_assert (mlib_maxof (uint8_t) == UINT8_MAX);
   mlib_static_assert (mlib_minof (uint16_t) == 0);
   mlib_static_assert (mlib_maxof (uint16_t) == UINT16_MAX);
   mlib_static_assert (mlib_minof (uint32_t) == 0);
   mlib_static_assert (mlib_maxof (uint32_t) == UINT32_MAX);
   mlib_static_assert (mlib_minof (uint64_t) == 0);
   mlib_static_assert (mlib_maxof (uint64_t) == UINT64_MAX);

   mlib_static_assert (mlib_maxof (size_t) == SIZE_MAX);
   mlib_static_assert (mlib_maxof (ptrdiff_t) == PTRDIFF_MAX);

   mlib_static_assert (mlib_minof (int) == INT_MIN);
   mlib_static_assert (mlib_maxof (int) == INT_MAX);
   mlib_static_assert (mlib_maxof (unsigned) == UINT_MAX);

   mlib_static_assert (mlib_is_signed (int));
   mlib_static_assert (mlib_is_signed (signed char));
   mlib_static_assert (mlib_is_signed (int8_t));
   mlib_static_assert (!mlib_is_signed (uint8_t));
   mlib_static_assert (mlib_is_signed (int16_t));
   mlib_static_assert (!mlib_is_signed (uint16_t));
   mlib_static_assert (mlib_is_signed (int32_t));
   mlib_static_assert (!mlib_is_signed (uint32_t));
   mlib_static_assert (mlib_is_signed (int64_t));
   mlib_static_assert (!mlib_is_signed (uint64_t));
   // Ambiguous signedness:
   mlib_static_assert (mlib_is_signed (char) || !mlib_is_signed (char));
}

static void
_test_upsize (void)
{
   struct mlib_upsized_integer up;
   up = mlib_upsize_integer (31);
   ASSERT (up.is_signed);
   ASSERT (up.i.s == 31);

   // Casting from the max unsigned integer generates an unsigned upsized integer:
   up = mlib_upsize_integer ((uintmax_t) 1729);
   ASSERT (!up.is_signed);
   ASSERT (up.i.u == 1729);

   // Max signed integer makes a signed upsized integer:
   up = mlib_upsize_integer ((intmax_t) 1729);
   ASSERT (up.is_signed);
   ASSERT (up.i.s == 1729);

   // From a literal:
   up = mlib_upsize_integer (UINTMAX_MAX);
   ASSERT (!up.is_signed);
   ASSERT (up.i.u == UINTMAX_MAX);
}

void
_test_cmp (void)
{
   ASSERT (mlib_cmp (1, 2) == mlib_less);
   ASSERT (mlib_cmp (1, 2) < 0);
   ASSERT (mlib_cmp (1, <, 2));
   ASSERT (mlib_cmp (2, 1) == mlib_greater);
   ASSERT (mlib_cmp (2, 1) > 0);
   ASSERT (mlib_cmp (2, >, 1));
   ASSERT (mlib_cmp (1, 1) == mlib_equal);
   ASSERT (mlib_cmp (1, 1) == 0);
   ASSERT (mlib_cmp (1, ==, 1));

   size_t big_size = SIZE_MAX;
   ASSERT (mlib_cmp (42, big_size) == mlib_less);
   ASSERT (mlib_cmp (big_size, big_size) == mlib_equal);
   ASSERT (mlib_cmp (big_size, SSIZE_MIN) == mlib_greater);
   uint8_t smol = 7;
   ASSERT (mlib_cmp (smol, SIZE_MAX) == mlib_less);
   int8_t ismol = -4;
   ASSERT (mlib_cmp (ismol, big_size) == mlib_less);

   /// Example: Getting the correct answer:
   // Unintuitive result due to integer promotion:
   mlib_diagnostic_push ();
   mlib_gnu_warning_disable ("-Wsign-compare");
   ASSERT (-27 > 20u);
   mlib_diagnostic_pop ();
   // mlib_cmp produces the correct answer:
   ASSERT (mlib_cmp (-27, <, 20u));
}

void
_test_in_range (void)
{
   ASSERT (!mlib_in_range (int8_t, 1729));
   ASSERT (!mlib_in_range (int, SIZE_MAX));
   ASSERT (mlib_in_range (size_t, SIZE_MAX));
   ASSERT (!mlib_in_range (size_t, -42));
   ASSERT (mlib_in_range (int8_t, -42));
   ASSERT (mlib_in_range (int8_t, -128));
   ASSERT (!mlib_in_range (int8_t, -129));
}

void
test_mlib_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/mlib/intutil/minmax", _test_minmax);
   TestSuite_Add (suite, "/mlib/intutil/upsize", _test_upsize);
   TestSuite_Add (suite, "/mlib/cmp", _test_cmp);
   TestSuite_Add (suite, "/mlib/in-range", _test_in_range);
}

mlib_diagnostic_pop ();
