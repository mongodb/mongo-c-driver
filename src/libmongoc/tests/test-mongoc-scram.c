#include <mongoc.h>

#include "mongoc-scram-private.h"

#include "TestSuite.h"

#ifdef MONGOC_ENABLE_SSL
static void
test_mongoc_scram_step_username_not_set (void)
{
   mongoc_scram_t scram;
   bool success;
   uint8_t buf[4096] = {0};
   uint32_t buflen = 0;
   bson_error_t error;

   _mongoc_scram_init (&scram);
   _mongoc_scram_set_pass (&scram, "password");

   success = _mongoc_scram_step (
      &scram, buf, buflen, buf, sizeof buf, &buflen, &error);

   ASSERT (!success);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SCRAM,
                          MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                          "SCRAM Failure: username is not set");

   _mongoc_scram_destroy (&scram);
}

typedef struct {
   const char *original;
   const char *normalized;
   bool should_be_required;
   bool should_succeed;
} sasl_prep_testcase_t;

static void
test_mongoc_scram_sasl_prep (void)
{
#ifdef MONGOC_ENABLE_ICU
   int i, ntests;
   char *normalized;
   bson_error_t err;
   /* examples from RFC 4013 section 3. */
   sasl_prep_testcase_t tests[] = {{"\x65\xCC\x81", "\xC3\xA9", true, true},
                                   {"I\xC2\xADX", "IX", true, true},
                                   {"user", "user", false, true},
                                   {"USER", "USER", false, true},
                                   {"\xC2\xAA", "a", true, true},
                                   {"\xE2\x85\xA8", "IX", true, true},
                                   {"\x07", "(invalid)", true, false},
                                   {"\xD8\xA7\x31", "(invalid)", true, false}};
   ntests = sizeof (tests) / sizeof (sasl_prep_testcase_t);
   for (i = 0; i < ntests; i++) {
      ASSERT_CMPINT (tests[i].should_be_required,
                     ==,
                     _mongoc_sasl_prep_required (tests[i].original));
      memset (&err, 0, sizeof (err));
      normalized = _mongoc_sasl_prep (
         "username", tests[i].original, strlen (tests[i].original), &err);
      if (tests[i].should_succeed) {
         ASSERT_CMPSTR (tests[i].normalized, normalized);
         ASSERT_CMPINT (err.code, ==, 0);
         bson_free (normalized);
      } else {
         ASSERT_CMPINT (err.code, ==, MONGOC_ERROR_SCRAM_PROTOCOL_ERROR);
         ASSERT_CMPINT (err.domain, ==, MONGOC_ERROR_SCRAM);
         BSON_ASSERT (normalized == NULL);
      }
   }
#endif
}
#endif

void
test_scram_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_Add (suite,
                  "/scram/username_not_set",
                  test_mongoc_scram_step_username_not_set);
   TestSuite_Add (suite, "/scram/sasl_prep", test_mongoc_scram_sasl_prep);
#endif
}
