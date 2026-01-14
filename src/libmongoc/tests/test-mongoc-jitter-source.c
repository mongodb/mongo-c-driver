#include <mongoc/mongoc-jitter-source-private.h>

#include <TestSuite.h>

#define ASSERT_CMPDURATION(a, op, b)                                                     \
   do {                                                                                  \
      const mlib_duration _a = (a);                                                      \
      const mlib_duration _b = (b);                                                      \
      if (!mlib_duration_cmp(_a, op, b)) {                                               \
         MONGOC_STDERR_PRINTF("FAIL\n\nAssert Failure: %" PRId64 "us %s %" PRId64 "us\n" \
                              "%s:%d  %s()\n",                                           \
                              mlib_microseconds_count(_a),                               \
                              BSON_STR(eq),                                              \
                              mlib_microseconds_count(_b),                               \
                              __FILE__,                                                  \
                              (int)(__LINE__),                                           \
                              BSON_FUNC);                                                \
         abort();                                                                        \
      }                                                                                  \
   } while (false)

#define ASSERT_DURATION_ALMOST_EQUAL(a, b)                                                              \
   do {                                                                                                 \
      const mlib_duration_rep_t _a = mlib_microseconds_count(a);                                        \
      const mlib_duration_rep_t _b = mlib_microseconds_count(b);                                        \
      const float _af = fabs((float)_a);                                                                \
      const float _bf = fabs((float)_b);                                                                \
      if (!(_af >= _bf * 0.99f && _af <= _bf * 1.01f)) {                                                \
         MONGOC_STDERR_PRINTF("FAIL\n\nAssert Failure: %" PRId64 "us not within 1%% of %" PRId64 "us\n" \
                              "%s:%d  %s()\n",                                                          \
                              _a,                                                                       \
                              _b,                                                                       \
                              __FILE__,                                                                 \
                              (int)(__LINE__),                                                          \
                              BSON_FUNC);                                                               \
         abort();                                                                                       \
      }                                                                                                 \
   } while (false)

static void
test_compute_backoff_duration(void)
{
   // jitter=0
   {
      const mlib_duration duration_zero = mlib_duration(0, us);

      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0f, 1), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0f, 2), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0f, 3), ==, duration_zero);
   }

   // jitter=1
   {
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 1), MONGOC_BACKOFF_INITIAL);
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 2), mlib_duration(7500, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 3), mlib_duration(11250, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 4), mlib_duration(16875, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 5), mlib_duration(25313, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 6), mlib_duration(37969, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 7), mlib_duration(56953, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 8), mlib_duration(85430, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 9), mlib_duration(128145, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 10), mlib_duration(192217, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 11), mlib_duration(288325, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 12), mlib_duration(432488, us));
      // After 13 retries, backoff should saturate to `MONGOC_BACKOFF_MAX`.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 13), MONGOC_BACKOFF_MAX);
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0f, 14), MONGOC_BACKOFF_MAX);
   }
}

void
test_jitter_source_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/jitter_source/compute_backoff_duration", test_compute_backoff_duration);
}
