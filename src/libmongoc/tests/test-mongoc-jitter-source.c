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

#define ASSERT_DURATION_ALMOST_EQUAL(lhs, rhs)                                                          \
   do {                                                                                                 \
      const mlib_duration_rep_t _lhs = mlib_microseconds_count(lhs);                                    \
      const mlib_duration_rep_t _rhs = mlib_microseconds_count(rhs);                                    \
      ASSERT_CMPINT64(_lhs, >=, 0);                                                                     \
      ASSERT_CMPINT64(_rhs, >=, 0);                                                                     \
      const double _lhs_d = (double)_lhs;                                                               \
      const double _rhs_d = (double)_rhs;                                                               \
      if (!(_lhs_d >= _rhs_d * 0.99 && _lhs_d <= _rhs_d * 1.01)) {                                      \
         MONGOC_STDERR_PRINTF("FAIL\n\nAssert Failure: %" PRId64 "us not within 1%% of %" PRId64 "us\n" \
                              "%s:%d  %s()\n",                                                          \
                              _lhs,                                                                     \
                              _rhs,                                                                     \
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

      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0, 1), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0, 2), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_compute_backoff_duration(0.0, 3), ==, duration_zero);
   }

   // jitter=0.5
   {
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 1),
                                   mlib_duration(MONGOC_BACKOFF_INITIAL, div, 2));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 2), mlib_duration(3750, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 3), mlib_duration(5625, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 4), mlib_duration(8438, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 5), mlib_duration(12657, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 6), mlib_duration(18985, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 7), mlib_duration(28477, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 8), mlib_duration(42715, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 9), mlib_duration(64073, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 10), mlib_duration(96109, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 11), mlib_duration(144163, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 12), mlib_duration(216244, us));
      // After 13 retries, backoff should saturate to `MONGOC_BACKOFF_MAX / 2`.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 13),
                                   mlib_duration(MONGOC_BACKOFF_MAX, div, 2));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(0.5, 14),
                                   mlib_duration(MONGOC_BACKOFF_MAX, div, 2));
   }

   // jitter=1
   {
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 1), MONGOC_BACKOFF_INITIAL);
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 2), mlib_duration(7500, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 3), mlib_duration(11250, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 4), mlib_duration(16875, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 5), mlib_duration(25313, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 6), mlib_duration(37969, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 7), mlib_duration(56953, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 8), mlib_duration(85430, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 9), mlib_duration(128145, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 10), mlib_duration(192217, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 11), mlib_duration(288325, us));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 12), mlib_duration(432488, us));
      // After 13 retries, backoff should saturate to `MONGOC_BACKOFF_MAX`.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 13), MONGOC_BACKOFF_MAX);
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_compute_backoff_duration(1.0, 14), MONGOC_BACKOFF_MAX);
   }
}

void
test_jitter_source_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/jitter_source/compute_backoff_duration", test_compute_backoff_duration);
}
