#include "TestSuite.h"

#include <mlib/intutil.h>

#include <stddef.h>

static void
_test_minmax (void)
{
   ASSERT (mlib_minof (unsigned) == 0);
   // Ambiguous signedness, still works:
   ASSERT (mlib_minof (char) == CHAR_MIN);
   ASSERT (mlib_maxof (char) == CHAR_MAX);

   ASSERT (mlib_minof (uint8_t) == 0);
   ASSERT (mlib_maxof (uint8_t) == UINT8_MAX);
   ASSERT (mlib_minof (uint16_t) == 0);
   ASSERT (mlib_maxof (uint16_t) == UINT16_MAX);
   ASSERT (mlib_minof (uint32_t) == 0);
   ASSERT (mlib_maxof (uint32_t) == UINT32_MAX);
   ASSERT (mlib_minof (uint64_t) == 0);
   ASSERT (mlib_maxof (uint64_t) == UINT64_MAX);

   ASSERT (mlib_maxof (size_t) == SIZE_MAX);
   ASSERT (mlib_maxof (ptrdiff_t) == PTRDIFF_MAX);

   ASSERT (mlib_minof (int) == INT_MIN);
   ASSERT (mlib_maxof (int) == INT_MAX);
   ASSERT (mlib_maxof (unsigned) == UINT_MAX);

   ASSERT (mlib_is_signed (int));
   ASSERT (mlib_is_signed (signed char));
   ASSERT (mlib_is_signed (int8_t));
   ASSERT (!mlib_is_signed (uint8_t));
   ASSERT (mlib_is_signed (int16_t));
   ASSERT (!mlib_is_signed (uint16_t));
   ASSERT (mlib_is_signed (int32_t));
   ASSERT (!mlib_is_signed (uint32_t));
   ASSERT (mlib_is_signed (int64_t));
   ASSERT (!mlib_is_signed (uint64_t));
   // Ambiguous signedness:
   ASSERT (mlib_is_signed (char) || !mlib_is_signed (char));
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
test_mlib_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/mlib/intutil/minmax", _test_minmax);
   TestSuite_Add (suite, "/mlib/intutil/upsize", _test_upsize);
}
