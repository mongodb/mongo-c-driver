/*
 * Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <bson/bson.h>

#include "TestSuite.h"
#include "test-libmongoc.h"


static void
test_bson_string_new (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new (NULL);
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, ""));
   bson_free (s);

   str = bson_string_new ("");
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!*s);
   BSON_ASSERT (0 == strcmp (s, ""));
   bson_free (s);

   str = bson_string_new ("abcdef");
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, "abcdef"));
   bson_free (s);

   str = bson_string_new ("");
   s = bson_string_free (str, true);
   BSON_ASSERT (!s);
}


static void
test_bson_string_append (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new (NULL);
   bson_string_append (str, "christian was here");
   bson_string_append (str, "\n");
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, "christian was here\n"));
   bson_free (s);

   str = bson_string_new (">>>");
   bson_string_append (str, "^^^");
   bson_string_append (str, "<<<");
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, ">>>^^^<<<"));
   bson_free (s);
}


static void
test_bson_string_append_c (void)
{
   bson_string_t *str;
   char *s;

   str = bson_string_new (NULL);
   bson_string_append_c (str, 'c');
   bson_string_append_c (str, 'h');
   bson_string_append_c (str, 'r');
   bson_string_append_c (str, 'i');
   bson_string_append_c (str, 's');
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, "chris"));
   bson_free (s);
}


static void
test_bson_string_append_printf (void)
{
   bson_string_t *str;

   str = bson_string_new ("abcd ");
   bson_string_append_printf (str, "%d %d %d", 1, 2, 3);
   BSON_ASSERT (!strcmp (str->str, "abcd 1 2 3"));
   bson_string_truncate (str, 2);
   BSON_ASSERT (!strcmp (str->str, "ab"));
   bson_string_free (str, true);
}


static void
test_bson_string_append_unichar (void)
{
   static const unsigned char test1[] = {0xe2, 0x82, 0xac, 0};
   bson_string_t *str;
   char *s;

   str = bson_string_new (NULL);
   bson_string_append_unichar (str, 0x20AC);
   s = bson_string_free (str, false);
   BSON_ASSERT (s);
   BSON_ASSERT (!strcmp (s, (const char *) test1));
   bson_free (s);
}


static void
test_bson_strdup_printf (void)
{
   char *s;

   s = bson_strdup_printf ("%s:%u", "localhost", 27017);
   BSON_ASSERT (!strcmp (s, "localhost:27017"));
   bson_free (s);
}


static void
test_bson_strdup (void)
{
   char *s;

   s = bson_strdup ("localhost:27017");
   BSON_ASSERT (!strcmp (s, "localhost:27017"));
   bson_free (s);
}


static void
test_bson_strndup (void)
{
   char *s;

   s = bson_strndup ("asdf", 2);
   BSON_ASSERT (!strcmp (s, "as"));
   bson_free (s);

   s = bson_strndup ("asdf", 10);
   BSON_ASSERT (!strcmp (s, "asdf"));
   bson_free (s);

   /* Some tests where we truncate to size n-1, n, n+1 */
   s = bson_strndup ("asdf", 3);
   BSON_ASSERT (!strcmp (s, "asd"));
   bson_free (s);

   s = bson_strndup ("asdf", 4);
   BSON_ASSERT (!strcmp (s, "asdf"));
   bson_free (s);

   s = bson_strndup ("asdf", 5);
   BSON_ASSERT (!strcmp (s, "asdf"));
   bson_free (s);
}


static void
test_bson_strnlen (void)
{
   char *s = "test";

   ASSERT_CMPINT ((int) strlen (s), ==, (int) bson_strnlen (s, 100));
}


typedef struct {
   const char *str;
   int base;
   int64_t rv;
   const char *remaining;
   int _errno;
} strtoll_test;


static void
test_bson_ascii_strtoll (void)
{
#ifdef END
#undef END
#endif
#define END ""
   int64_t rv;
   int i;
   char *endptr;
   strtoll_test tests[] = {/* input, base, expected output, # of chars parsed, expected errno */
                           {"1", 10, 1, END, 0},
                           {"+1", 10, 1, END, 0},
                           {"-1", 10, -1, END, 0},
                           {"0", 10, 0, END, 0},
                           {"0 ", 10, 0, " ", 0},
                           {" 0 ", 10, 0, " ", 0},
                           {" 0", 10, 0, END, 0},
                           {" 0\"", 10, 0, "\"", 0},
                           {"0l", 10, 0, "l", 0},
                           {"0l ", 10, 0, "l ", 0},
                           {"0u", 10, 0, "u", 0},
                           {"0u ", 10, 0, "u ", 0},
                           {"0L", 10, 0, "L", 0},
                           {"0L ", 10, 0, "L ", 0},
                           {"0U", 10, 0, "U", 0},
                           {"0U ", 10, 0, "U ", 0},
                           {"-0", 10, 0, END, 0},
                           {"+0", 10, 0, END, 0},
                           {"010", 8, 8, END, 0},
                           /* stroll "takes as many characters as possible to form a valid base-n
                            * integer", so it ignores "8" and returns 0 */
                           {"08", 0, 0, "8", 0},
                           {"010", 10, 10, END, 0},
                           {"010", 8, 8, END, 0},
                           {"010", 0, 8, END, 0},
                           {"68719476736", 10, 68719476736, END, 0},
                           {"-68719476736", 10, -68719476736, END, 0},
                           {"+68719476736", 10, 68719476736, END, 0},
                           {"   68719476736  ", 10, 68719476736, "  ", 0},
                           {"   68719476736  ", 0, 68719476736, "  ", 0},
                           {"   -68719476736  ", 10, -68719476736, "  ", 0},
                           {"   -68719476736  ", 0, -68719476736, "  ", 0},
                           {"   4611686018427387904LL", 10, 4611686018427387904LL, "LL", 0},
                           {" -4611686018427387904LL ", 10, -4611686018427387904LL, "LL ", 0},
                           {"0x1000000000", 16, 68719476736, END, 0},
                           {"0x1000000000", 0, 68719476736, END, 0},
                           {"-0x1000000000", 16, -68719476736, END, 0},
                           {"-0x1000000000", 0, -68719476736, END, 0},
                           {"+0x1000000000", 16, 68719476736, END, 0},
                           {"+0x1000000000", 0, 68719476736, END, 0},
                           {"01234", 8, 668, END, 0},
                           {"01234", 0, 668, END, 0},
                           {"-01234", 8, -668, END, 0},
                           {"-01234", 0, -668, END, 0},
                           {"+01234", 8, 668, END, 0},
                           {"+01234", 0, 668, END, 0},
                           {"9223372036854775807", 10, INT64_MAX, END, 0},
                           {"-9223372036854775808", 10, INT64_MIN, END, 0},
                           {"9223372036854775808", 10, INT64_MAX, "8", ERANGE},   /* LLONG_MAX+1   */
                           {"-9223372036854775809", 10, INT64_MIN, "9", ERANGE},  /* LLONG_MIN-1   */
                           {"18446744073709551615", 10, INT64_MAX, "5", ERANGE},  /* 2*LLONG_MAX+1 */
                           {"-18446744073709551618", 10, INT64_MIN, "8", ERANGE}, /* 2*LLONG_MIN-1 */
                           {NULL}};

   for (i = 0; tests[i].str; i++) {
      errno = 0;

      rv = bson_ascii_strtoll (tests[i].str, &endptr, tests[i].base);
      ASSERT_CMPINT64 (rv, ==, tests[i].rv);
      ASSERT_CMPINT (errno, ==, tests[i]._errno);
      ASSERT_CMPSTR (endptr, tests[i].remaining);
   }
#undef END
}


static void
test_bson_strncpy (void)
{
   char buf[5];

   bson_strncpy (buf, "foo", sizeof buf);
   ASSERT_CMPSTR ("foo", buf);
   bson_strncpy (buf, "foobar", sizeof buf);
   ASSERT_CMPSTR ("foob", buf);
   /* CDRIVER-2596 make sure strncpy with size 0 doesn't write to buf[-1] */
   bson_strncpy (buf + 1, "z", 0);
   ASSERT_CMPSTR ("foob", buf);
}


static void
test_bson_snprintf (void)
{
   char buf[] = "ab";

   /* CDRIVER-2595 make sure snprintf with size 0 doesn't write to buf[-1] */
   ASSERT_CMPINT (bson_snprintf (buf + 1, 0, "%d", 1), ==, 0);
   ASSERT_CMPSTR (buf, "ab");
}


static void
test_bson_strcasecmp (void)
{
   BSON_ASSERT (!bson_strcasecmp ("FoO", "foo"));
   BSON_ASSERT (bson_strcasecmp ("Foa", "foo") < 0);
   BSON_ASSERT (bson_strcasecmp ("FoZ", "foo") > 0);
}

static void
test_bson_string_truncate (void)
{
   // Test truncating.
   {
      bson_string_t *str = bson_string_new ("foobar");
      ASSERT_CMPSIZE_T (str->len, ==, 6u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 8u);

      bson_string_truncate (str, 2);
      ASSERT_CMPSTR (str->str, "fo");
      ASSERT_CMPSIZE_T (str->len, ==, 2u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 4u);
      bson_string_free (str, true);
   }

   // Test truncating to same length.
   {
      bson_string_t *str = bson_string_new ("foobar");
      ASSERT_CMPSIZE_T (str->len, ==, 6u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 8u);

      bson_string_truncate (str, 6u);
      ASSERT_CMPSTR (str->str, "foobar");
      ASSERT_CMPUINT32 (str->len, ==, 6u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 8u);
      bson_string_free (str, true);
   }

   // Test truncating to 0.
   {
      bson_string_t *str = bson_string_new ("foobar");
      ASSERT_CMPSIZE_T (str->len, ==, 6u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 8u);

      bson_string_truncate (str, 0u);
      ASSERT_CMPSTR (str->str, "");
      ASSERT_CMPUINT32 (str->len, ==, 0u);
      ASSERT_CMPSIZE_T (str->alloc, ==, 1u);
      bson_string_free (str, true);
   }

   // Test truncating beyond string length.
   // The documentation for `bson_string_truncate` says the truncated length "must be smaller or equal to the current
   // length of the string.". However, `bson_string_truncate` does not reject greater lengths. For backwards
   // compatibility, this behavior is preserved.
   {
      bson_string_t *str = bson_string_new ("a");
      bson_string_truncate (str, 2u); // Is not rejected.
      ASSERT_CMPUINT32 (str->len, ==, 2u);
      ASSERT_CMPUINT32 (str->alloc, ==, 4u);
      bson_string_free (str, true);
   }
}

static void
test_bson_string_capacity (void *unused)
{
   BSON_UNUSED (unused);

   char *large_str = bson_malloc (UINT32_MAX);
   memset (large_str, 's', UINT32_MAX); // Do not NULL terminate. Each test case sets NULL byte.

   // Test the largest possible string that can be constructed.
   {
      large_str[UINT32_MAX - 1u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new (large_str);
      bson_string_free (str, true);
      large_str[UINT32_MAX - 1u] = 's'; // Restore.
   }

   // Test appending with `bson_string_append` to get maximum size.
   {
      large_str[UINT32_MAX - 1u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new ("");
      bson_string_append (str, large_str);
      bson_string_free (str, true);
      large_str[UINT32_MAX - 1u] = 's'; // Restore.
   }

   // Test appending with `bson_string_append_c` to get maximum size.
   {
      large_str[UINT32_MAX - 2u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new (large_str);
      bson_string_append_c (str, 'c');
      bson_string_free (str, true);
      large_str[UINT32_MAX - 2u] = 's'; // Restore.
   }

   // Test appending with `bson_string_append_printf` to get maximum size.
   {
      large_str[UINT32_MAX - 2u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new (large_str);
      bson_string_append_printf (str, "c");
      bson_string_free (str, true);
      large_str[UINT32_MAX - 2u] = 's'; // Restore.
   }

   // Test appending with single characters.
   {
      large_str[UINT32_MAX - 2u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new (large_str);
      bson_string_append_unichar (str, (bson_unichar_t) 's');
      bson_string_free (str, true);
      large_str[UINT32_MAX - 2u] = 's'; // Restore.
   }

   // Can truncate strings of length close to UINT32_MAX - 1.
   {
      large_str[UINT32_MAX - 1u] = '\0'; // Set size.
      bson_string_t *str = bson_string_new (large_str);
      bson_string_truncate (str, UINT32_MAX - 2); // Truncate one character.
      ASSERT_CMPSIZE_T (strlen (str->str), ==, UINT32_MAX - 2u);
      bson_string_free (str, true);
      large_str[UINT32_MAX - 1u] = 's'; // Restore.
   }

   bson_free (large_str);
}

static int
skip_if_no_large_allocations (void)
{
   // Skip tests requiring large allocations.
   // Large allocations were observed to fail when run with TSan, and are time consuming with ASan.
   return test_framework_getenv_bool ("MONGOC_TEST_LARGE_ALLOCATIONS");
}

void
test_string_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/bson/string/new", test_bson_string_new);
   TestSuite_Add (suite, "/bson/string/append", test_bson_string_append);
   TestSuite_Add (suite, "/bson/string/append_c", test_bson_string_append_c);
   TestSuite_Add (suite, "/bson/string/append_printf", test_bson_string_append_printf);
   TestSuite_Add (suite, "/bson/string/append_unichar", test_bson_string_append_unichar);
   TestSuite_Add (suite, "/bson/string/strdup", test_bson_strdup);
   TestSuite_Add (suite, "/bson/string/strdup_printf", test_bson_strdup_printf);
   TestSuite_Add (suite, "/bson/string/strndup", test_bson_strndup);
   TestSuite_Add (suite, "/bson/string/ascii_strtoll", test_bson_ascii_strtoll);
   TestSuite_Add (suite, "/bson/string/strncpy", test_bson_strncpy);
   TestSuite_Add (suite, "/bson/string/snprintf", test_bson_snprintf);
   TestSuite_Add (suite, "/bson/string/strnlen", test_bson_strnlen);
   TestSuite_Add (suite, "/bson/string/strcasecmp", test_bson_strcasecmp);
   TestSuite_AddFull (
      suite, "/bson/string/capacity", test_bson_string_capacity, NULL, NULL, skip_if_no_large_allocations);
   TestSuite_Add (suite, "/bson/string/truncate", test_bson_string_truncate);
}
