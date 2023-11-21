#include <mongoc/mongoc.h>
#include "mongoc/mongoc-util-private.h"
#include "TestSuite.h"
#include "test-libmongoc.h"


static void
test_mongoc_usleep_basic (void)
{
   int64_t start;
   int64_t duration;

   start = bson_get_monotonic_time ();
   _mongoc_usleep (50 * 1000); /* 50 ms */
   duration = bson_get_monotonic_time () - start;
   ASSERT_CMPINT ((int) duration, >, 0);
   ASSERT_CMPTIME ((int) duration, 200 * 1000);
}

static void
custom_usleep_impl (int64_t usec, void *user_data) {
   if (user_data) {
      *(int64_t*)user_data = usec;
   }
}

static void
test_mongoc_usleep_custom (void)
{
   static const int64_t expected = 42;
   int64_t last_sleep_dur = -1;

   void *old_usleep_data;
   mongoc_usleep_func_t old_usleep_fn = mongoc_usleep_set_impl(
      custom_usleep_impl, &last_sleep_dur, &old_usleep_data);

   _mongoc_usleep (expected);
   (void)mongoc_usleep_set_impl(old_usleep_fn, old_usleep_data, NULL);

   ASSERT_CMPINT64 (last_sleep_dur, =, expected);
}

void
test_usleep_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Sleep/basic", test_mongoc_usleep_basic);
   TestSuite_Add (suite, "/Sleep/custom", test_mongoc_usleep_custom);
}
