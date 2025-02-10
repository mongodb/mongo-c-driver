#include "TestSuite.h"

#include <mlib/intutil.h>
#include <mlib/config.h>
#include <mlib/intencode.h>
#include <mlib/loop.h>
#include <mlib/cmp.h>
#include <mlib/test.h>

#include <stddef.h>

mlib_diagnostic_push (); // We don't set any diagnostics, we just want to make sure it compiles

// Not relevant, we just want to test that it compiles:
mlib_msvc_warning (disable : 4507);

static void
_test_checks (void)
{
   // Simple condiion
   mlib_check (true);
   mlib_assert_aborts () {
      mlib_check (false);
   }
   // streq
   mlib_check ("foo", streq, "foo");
   mlib_assert_aborts () {
      mlib_check ("foo", streq, "bar");
   }
   // eq
   mlib_check (4, eq, 4);
   mlib_assert_aborts () {
      mlib_check (1, eq, 4);
   }
   // neq
   mlib_check (1, neq, 4);
   mlib_assert_aborts () {
      mlib_check (1, neq, 1);
   }
}

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
   mlib_check (mlib_cmp (1, 2) == mlib_less);
   mlib_check (mlib_cmp (1, 2) < 0);
   mlib_check (mlib_cmp (1, <, 2));
   mlib_check (mlib_cmp (2, 1) == mlib_greater);
   mlib_check (mlib_cmp (2, 1) > 0);
   mlib_check (mlib_cmp (2, >, 1));
   mlib_check (mlib_cmp (1, 1) == mlib_equal);
   mlib_check (mlib_cmp (1, 1) == 0);
   mlib_check (mlib_cmp (1, ==, 1));

   ASSERT (mlib_cmp (0, ==, 0));
   ASSERT (!mlib_cmp (0, ==, -1));
   ASSERT (!mlib_cmp (0, ==, 1));
   ASSERT (!mlib_cmp (-1, ==, 0));
   ASSERT (mlib_cmp (-1, ==, -1));
   ASSERT (!mlib_cmp (-1, ==, 1));
   ASSERT (!mlib_cmp (1, ==, 0));
   ASSERT (!mlib_cmp (1, ==, -1));
   ASSERT (mlib_cmp (1, ==, 1));

   ASSERT (mlib_cmp (0u, ==, 0u));
   ASSERT (!mlib_cmp (0u, ==, 1u));
   ASSERT (!mlib_cmp (1u, ==, 0u));
   ASSERT (mlib_cmp (1u, ==, 1u));

   ASSERT (mlib_cmp (0, ==, 0u));
   ASSERT (!mlib_cmp (0, ==, 1u));
   ASSERT (!mlib_cmp (-1, ==, 0u));
   ASSERT (!mlib_cmp (-1, ==, 1u));
   ASSERT (!mlib_cmp (1, ==, 0u));
   ASSERT (mlib_cmp (1, ==, 1u));

   ASSERT (mlib_cmp (0u, ==, 0));
   ASSERT (!mlib_cmp (0u, ==, -1));
   ASSERT (!mlib_cmp (0u, ==, 1));
   ASSERT (!mlib_cmp (1u, ==, 0));
   ASSERT (!mlib_cmp (1u, ==, -1));
   ASSERT (mlib_cmp (1u, ==, 1));

   ASSERT (!mlib_cmp (0, !=, 0));
   ASSERT (mlib_cmp (0, !=, -1));
   ASSERT (mlib_cmp (0, !=, 1));
   ASSERT (mlib_cmp (-1, !=, 0));
   ASSERT (!mlib_cmp (-1, !=, -1));
   ASSERT (mlib_cmp (-1, !=, 1));
   ASSERT (mlib_cmp (1, !=, 0));
   ASSERT (mlib_cmp (1, !=, -1));
   ASSERT (!mlib_cmp (1, !=, 1));

   ASSERT (!mlib_cmp (0u, !=, 0u));
   ASSERT (mlib_cmp (0u, !=, 1u));
   ASSERT (mlib_cmp (1u, !=, 0u));
   ASSERT (!mlib_cmp (1u, !=, 1u));

   ASSERT (!mlib_cmp (0, !=, 0u));
   ASSERT (mlib_cmp (0, !=, 1u));
   ASSERT (mlib_cmp (-1, !=, 0u));
   ASSERT (mlib_cmp (-1, !=, 1u));
   ASSERT (mlib_cmp (1, !=, 0u));
   ASSERT (!mlib_cmp (1, !=, 1u));

   ASSERT (!mlib_cmp (0u, !=, 0));
   ASSERT (mlib_cmp (0u, !=, -1));
   ASSERT (mlib_cmp (0u, !=, 1));
   ASSERT (mlib_cmp (1u, !=, 0));
   ASSERT (mlib_cmp (1u, !=, -1));
   ASSERT (!mlib_cmp (1u, !=, 1));

   ASSERT (!mlib_cmp (0, <, 0));
   ASSERT (!mlib_cmp (0, <, -1));
   ASSERT (mlib_cmp (0, <, 1));
   ASSERT (mlib_cmp (-1, <, 0));
   ASSERT (!mlib_cmp (-1, <, -1));
   ASSERT (mlib_cmp (-1, <, 1));
   ASSERT (!mlib_cmp (1, <, 0));
   ASSERT (!mlib_cmp (1, <, -1));
   ASSERT (!mlib_cmp (1, <, 1));

   ASSERT (!mlib_cmp (0u, <, 0u));
   ASSERT (mlib_cmp (0u, <, 1u));
   ASSERT (!mlib_cmp (1u, <, 0u));
   ASSERT (!mlib_cmp (1u, <, 1u));

   ASSERT (!mlib_cmp (0, <, 0u));
   ASSERT (mlib_cmp (0, <, 1u));
   ASSERT (mlib_cmp (-1, <, 0u));
   ASSERT (mlib_cmp (-1, <, 1u));
   ASSERT (!mlib_cmp (1, <, 0u));
   ASSERT (!mlib_cmp (1, <, 1u));

   ASSERT (!mlib_cmp (0u, <, 0));
   ASSERT (!mlib_cmp (0u, <, -1));
   ASSERT (mlib_cmp (0u, <, 1));
   ASSERT (!mlib_cmp (1u, <, 0));
   ASSERT (!mlib_cmp (1u, <, -1));
   ASSERT (!mlib_cmp (1u, <, 1));

   ASSERT (!mlib_cmp (0, >, 0));
   ASSERT (mlib_cmp (0, >, -1));
   ASSERT (!mlib_cmp (0, >, 1));
   ASSERT (!mlib_cmp (-1, >, 0));
   ASSERT (!mlib_cmp (-1, >, -1));
   ASSERT (!mlib_cmp (-1, >, 1));
   ASSERT (mlib_cmp (1, >, 0));
   ASSERT (mlib_cmp (1, >, -1));
   ASSERT (!mlib_cmp (1, >, 1));

   ASSERT (!mlib_cmp (0u, >, 0u));
   ASSERT (!mlib_cmp (0u, >, 1u));
   ASSERT (mlib_cmp (1u, >, 0u));
   ASSERT (!mlib_cmp (1u, >, 1u));

   ASSERT (!mlib_cmp (0, >, 0u));
   ASSERT (!mlib_cmp (0, >, 1u));
   ASSERT (!mlib_cmp (-1, >, 0u));
   ASSERT (!mlib_cmp (-1, >, 1u));
   ASSERT (mlib_cmp (1, >, 0u));
   ASSERT (!mlib_cmp (1, >, 1u));

   ASSERT (!mlib_cmp (0u, >, 0));
   ASSERT (mlib_cmp (0u, >, -1));
   ASSERT (!mlib_cmp (0u, >, 1));
   ASSERT (mlib_cmp (1u, >, 0));
   ASSERT (mlib_cmp (1u, >, -1));
   ASSERT (!mlib_cmp (1u, >, 1));

   ASSERT (mlib_cmp (0, <=, 0));
   ASSERT (!mlib_cmp (0, <=, -1));
   ASSERT (mlib_cmp (0, <=, 1));
   ASSERT (mlib_cmp (-1, <=, 0));
   ASSERT (mlib_cmp (-1, <=, -1));
   ASSERT (mlib_cmp (-1, <=, 1));
   ASSERT (!mlib_cmp (1, <=, 0));
   ASSERT (!mlib_cmp (1, <=, -1));
   ASSERT (mlib_cmp (1, <=, 1));

   ASSERT (mlib_cmp (0u, <=, 0u));
   ASSERT (mlib_cmp (0u, <=, 1u));
   ASSERT (!mlib_cmp (1u, <=, 0u));
   ASSERT (mlib_cmp (1u, <=, 1u));

   ASSERT (mlib_cmp (0, <=, 0u));
   ASSERT (mlib_cmp (0, <=, 1u));
   ASSERT (mlib_cmp (-1, <=, 0u));
   ASSERT (mlib_cmp (-1, <=, 1u));
   ASSERT (!mlib_cmp (1, <=, 0u));
   ASSERT (mlib_cmp (1, <=, 1u));

   ASSERT (mlib_cmp (0u, <=, 0));
   ASSERT (!mlib_cmp (0u, <=, -1));
   ASSERT (mlib_cmp (0u, <=, 1));
   ASSERT (!mlib_cmp (1u, <=, 0));
   ASSERT (!mlib_cmp (1u, <=, -1));
   ASSERT (mlib_cmp (1u, <=, 1));

   ASSERT (mlib_cmp (0, >=, 0));
   ASSERT (mlib_cmp (0, >=, -1));
   ASSERT (!mlib_cmp (0, >=, 1));
   ASSERT (!mlib_cmp (-1, >=, 0));
   ASSERT (mlib_cmp (-1, >=, -1));
   ASSERT (!mlib_cmp (-1, >=, 1));
   ASSERT (mlib_cmp (1, >=, 0));
   ASSERT (mlib_cmp (1, >=, -1));
   ASSERT (mlib_cmp (1, >=, 1));

   ASSERT (mlib_cmp (0u, >=, 0u));
   ASSERT (!mlib_cmp (0u, >=, 1u));
   ASSERT (mlib_cmp (1u, >=, 0u));
   ASSERT (mlib_cmp (1u, >=, 1u));

   ASSERT (mlib_cmp (0, >=, 0u));
   ASSERT (!mlib_cmp (0, >=, 1u));
   ASSERT (!mlib_cmp (-1, >=, 0u));
   ASSERT (!mlib_cmp (-1, >=, 1u));
   ASSERT (mlib_cmp (1, >=, 0u));
   ASSERT (mlib_cmp (1, >=, 1u));

   ASSERT (mlib_cmp (0u, >=, 0));
   ASSERT (mlib_cmp (0u, >=, -1));
   ASSERT (!mlib_cmp (0u, >=, 1));
   ASSERT (mlib_cmp (1u, >=, 0));
   ASSERT (mlib_cmp (1u, >=, -1));
   ASSERT (mlib_cmp (1u, >=, 1));

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
   const int64_t int8_min = INT8_MIN;
   const int64_t int8_max = INT8_MAX;
   const int64_t int32_min = INT32_MIN;
   const int64_t int32_max = INT32_MAX;

   const uint64_t uint8_max = UINT8_MAX;
   const uint64_t uint32_max = UINT32_MAX;

   const ssize_t ssize_min = SSIZE_MIN;
   const ssize_t ssize_max = SSIZE_MAX;

   ASSERT (!mlib_in_range (int8_t, 1729));
   ASSERT (!mlib_in_range (int, SIZE_MAX));
   ASSERT (mlib_in_range (size_t, SIZE_MAX));
   ASSERT (!mlib_in_range (size_t, -42));
   ASSERT (mlib_in_range (int8_t, -42));
   ASSERT (mlib_in_range (int8_t, -128));
   ASSERT (!mlib_in_range (int8_t, -129));

   ASSERT (!mlib_in_range (int8_t, int8_min - 1));
   ASSERT (mlib_in_range (int8_t, int8_min));
   ASSERT (mlib_in_range (int8_t, 0));
   ASSERT (mlib_in_range (int8_t, int8_max));
   ASSERT (!mlib_in_range (int8_t, int8_max + 1));

   ASSERT (mlib_in_range (int8_t, 0u));
   ASSERT (mlib_in_range (int8_t, (uint64_t) int8_max));
   ASSERT (!mlib_in_range (int8_t, (uint64_t) (int8_max + 1)));

   ASSERT (!mlib_in_range (uint8_t, int8_min - 1));
   ASSERT (!mlib_in_range (uint8_t, int8_min));
   ASSERT (mlib_in_range (uint8_t, 0));
   ASSERT (mlib_in_range (uint8_t, int8_max));
   ASSERT (mlib_in_range (uint8_t, int8_max + 1));
   ASSERT (mlib_in_range (uint8_t, (int64_t) uint8_max));
   ASSERT (!mlib_in_range (uint8_t, (int64_t) uint8_max + 1));

   ASSERT (mlib_in_range (uint8_t, 0u));
   ASSERT (mlib_in_range (uint8_t, uint8_max));
   ASSERT (!mlib_in_range (uint8_t, uint8_max + 1u));

   ASSERT (!mlib_in_range (int32_t, int32_min - 1));
   ASSERT (mlib_in_range (int32_t, int32_min));
   ASSERT (mlib_in_range (int32_t, 0));
   ASSERT (mlib_in_range (int32_t, int32_max));
   ASSERT (!mlib_in_range (int32_t, int32_max + 1));

   ASSERT (mlib_in_range (int32_t, 0u));
   ASSERT (mlib_in_range (int32_t, (uint64_t) int32_max));
   ASSERT (!mlib_in_range (int32_t, (uint64_t) (int32_max + 1)));

   ASSERT (!mlib_in_range (uint32_t, int32_min - 1));
   ASSERT (!mlib_in_range (uint32_t, int32_min));
   ASSERT (mlib_in_range (uint32_t, 0));
   ASSERT (mlib_in_range (uint32_t, int32_max));
   ASSERT (mlib_in_range (uint32_t, int32_max + 1));
   ASSERT (mlib_in_range (uint32_t, (int64_t) uint32_max));
   ASSERT (!mlib_in_range (uint32_t, (int64_t) uint32_max + 1));

   ASSERT (mlib_in_range (uint32_t, 0u));
   ASSERT (mlib_in_range (uint32_t, uint32_max));
   ASSERT (!mlib_in_range (uint32_t, uint32_max + 1u));

   ASSERT (mlib_in_range (ssize_t, ssize_min));
   ASSERT (mlib_in_range (ssize_t, 0));
   ASSERT (mlib_in_range (ssize_t, ssize_max));

   ASSERT (mlib_in_range (ssize_t, 0u));
   ASSERT (mlib_in_range (ssize_t, (size_t) ssize_max));
   ASSERT (!mlib_in_range (ssize_t, (size_t) ssize_max + 1u));

   ASSERT (!mlib_in_range (size_t, ssize_min));
   ASSERT (mlib_in_range (size_t, 0));
   ASSERT (mlib_in_range (size_t, ssize_max));

   ASSERT (mlib_in_range (size_t, 0u));
   ASSERT (mlib_in_range (size_t, (size_t) ssize_max));
   ASSERT (mlib_in_range (size_t, (size_t) ssize_max + 1u));
}

void
_test_assert_aborts (void)
{
   int a = 0;
   mlib_assert_aborts () {
      a = 4;
      abort ();
   }
   // Parent process is unaffected:
   ASSERT (a == 0);
}

void
_test_int_encoding (void)
{
   {
      const char *buf = "\x01\x02\x03\x04";
      const uint32_t val = mlib_read_u32le (buf);
      mlib_check (val, eq, 0x04030201);
   }

   {
      char buf[9] = {0};
      char *o = mlib_write_i32le (buf, 0x01020304);
      mlib_check (o, ptreq, buf + 4);
      mlib_check (buf, streq, "\x04\x03\x02\x01");

      o = mlib_write_i32le (o, 42);
      mlib_check (o, ptreq, buf + 8);
      mlib_check (buf, streq, "\x04\x03\x02\x01*");

      o = mlib_write_i64le (buf, 0x0102030405060708);
      mlib_check (o, ptreq, buf + 8);
      mlib_check (buf, streq, "\x08\x07\x06\x05\x04\x03\x02\x01");
   }
}

void
_test_foreach (void)
{
   int n_loops = 0;
   mlib_foreach_urange (i, 10) {
      fprintf (stderr, "i: %zu\n", i);
      fprintf (stderr, "counter: %zu\n", i_counter);
      ASSERT (i == loop.index);
      ASSERT (loop.first == (i == 0));
      ASSERT (loop.last == (i == 9));
      ++n_loops;
      (void) i;
      ASSERT (n_loops <= 10);
   };
   ASSERT (n_loops == 10);

   n_loops = 0;
   mlib_foreach_urange (i, 100) {
      if (i == 42) {
         break;
      }
      ++n_loops;
   }
   ASSERT (n_loops == 42);

   n_loops = 0;
   mlib_foreach_urange (i, 1729) {
      (void) i;
      ++n_loops;
   }
   ASSERT (n_loops == 1729);

   mlib_foreach_urange (i, 0) {
      (void) i;
      ASSERT (false); // Shouldn't ever enter the loop
   }

   n_loops = 0;
   mlib_foreach_urange (i, 4, 7) {
      ++n_loops;
      ASSERT (i >= 4);
      ASSERT (i < 7);
   }
   ASSERT (n_loops == 3);

   int arr[] = {1, 2, 3};
   int sum = 0;
   n_loops = 0;
   mlib_foreach_arr (int, n, arr) {
      mlib_check (n_loops, eq, loop.index);
      n_loops++;
      sum += *n;
      ASSERT (loop.first == (n == arr + 0));
      ASSERT (loop.last == (n == arr + 2));
   }
   ASSERT (sum == 6);
   ASSERT (n_loops == 3);
}

void
test_mlib_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/mlib/checks", _test_checks);
   TestSuite_Add (suite, "/mlib/intutil/minmax", _test_minmax);
   TestSuite_Add (suite, "/mlib/intutil/upsize", _test_upsize);
   TestSuite_Add (suite, "/mlib/cmp", _test_cmp);
   TestSuite_Add (suite, "/mlib/in-range", _test_in_range);
   TestSuite_Add (suite, "/mlib/assert-aborts", _test_assert_aborts);
   TestSuite_Add (suite, "/mlib/int-encoding", _test_int_encoding);
   TestSuite_Add (suite, "/mlib/foreach", _test_foreach);
}

mlib_diagnostic_pop ();
