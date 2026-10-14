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

#include <mongoc/mongoc-retry-backoff-generator-private.h>

#include <TestSuite.h>

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

static double
always_0_jitter_source_generate(mongoc_jitter_source_t *source)
{
   BSON_UNUSED(source);
   return 0.0;
}

static double
always_0_point_5_jitter_source_generate(mongoc_jitter_source_t *source)
{
   BSON_UNUSED(source);
   return 0.5;
}

static double
always_1_jitter_source_generate(mongoc_jitter_source_t *source)
{
   BSON_UNUSED(source);
   return 1.0;
}

static void
test_retry_backoff_generator(void)
{
   const mongoc_retry_backoff_params_t backoff_params = {
      .growth_factor = 2.0,
      .backoff_max = mlib_duration(10, s),
   };

   // jitter = 0
   {
      mongoc_jitter_source_t *const jitter_source = _mongoc_jitter_source_new(always_0_jitter_source_generate);
      mongoc_retry_backoff_generator_t *const generator =
         _mongoc_retry_backoff_generator_new(backoff_params, jitter_source);

      const mlib_duration base_backoff = mlib_duration(100, ms);
      const mlib_duration duration_zero = mlib_duration();

      ASSERT_CMPDURATION(_mongoc_retry_backoff_generator_next(generator, base_backoff), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_retry_backoff_generator_next(generator, base_backoff), ==, duration_zero);
      ASSERT_CMPDURATION(_mongoc_retry_backoff_generator_next(generator, base_backoff), ==, duration_zero);

      _mongoc_retry_backoff_generator_destroy(generator);
      _mongoc_jitter_source_destroy(jitter_source);
   }

   // jitter = 0.5
   {
      mongoc_jitter_source_t *const jitter_source = _mongoc_jitter_source_new(always_0_point_5_jitter_source_generate);
      mongoc_retry_backoff_generator_t *const generator =
         _mongoc_retry_backoff_generator_new(backoff_params, jitter_source);

      const mlib_duration base_backoff = mlib_duration(100, ms);

      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(100, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(200, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(400, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(800, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(1600, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(3200, ms));
      // On the 7th attempt, backoff should saturate to 5s (BACKOFF_MAX * 0.5).
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff), mlib_duration(5, s));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff), mlib_duration(5, s));

      _mongoc_retry_backoff_generator_destroy(generator);
      _mongoc_jitter_source_destroy(jitter_source);
   }

   // jitter = 1.0
   {
      mongoc_jitter_source_t *const jitter_source = _mongoc_jitter_source_new(always_1_jitter_source_generate);
      mongoc_retry_backoff_generator_t *const generator =
         _mongoc_retry_backoff_generator_new(backoff_params, jitter_source);

      const mlib_duration base_backoff = mlib_duration(100, ms);

      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(200, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(400, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(800, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(1600, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(3200, ms));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff),
                                   mlib_duration(6400, ms));
      // On the 7th attempt, backoff should saturate to 10s.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff), mlib_duration(10, s));
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, base_backoff), mlib_duration(10, s));

      _mongoc_retry_backoff_generator_destroy(generator);
      _mongoc_jitter_source_destroy(jitter_source);
   }

   // jitter = 1.0, variable base backoff
   {
      mongoc_jitter_source_t *const jitter_source = _mongoc_jitter_source_new(always_1_jitter_source_generate);
      mongoc_retry_backoff_generator_t *const generator =
         _mongoc_retry_backoff_generator_new(backoff_params, jitter_source);

      // Attempt 1 with 100ms base backoff: 100ms * 2^1 = 200ms.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(100, ms)),
                                   mlib_duration(200, ms));

      // Attempt 2 with 50ms base backoff: 50ms * 2^2 = 200ms.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(50, ms)),
                                   mlib_duration(200, ms));

      // Attempt 3 with 400ms base backoff: 400ms * 2^3 = 3200ms.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(400, ms)),
                                   mlib_duration(3200, ms));

      // Attempt 4 with 800ms base backoff: 800ms * 2^4 = 12800ms (saturates to 10s).
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(800, ms)),
                                   mlib_duration(10, s));

      // Attempt 5 with 300ms base backoff: 300ms * 2^5 = 9600ms (should not saturate).
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(300, ms)),
                                   mlib_duration(9600, ms));

      _mongoc_retry_backoff_generator_destroy(generator);
      _mongoc_jitter_source_destroy(jitter_source);
   }

   // jitter = 1.0, first attempt skipped
   {
      mongoc_jitter_source_t *const jitter_source = _mongoc_jitter_source_new(always_1_jitter_source_generate);
      mongoc_retry_backoff_generator_t *const generator =
         _mongoc_retry_backoff_generator_new(backoff_params, jitter_source);

      _mongoc_retry_backoff_generator_skip(generator);

      // Attempt 2: 100ms * 2^2 = 400ms.
      ASSERT_DURATION_ALMOST_EQUAL(_mongoc_retry_backoff_generator_next(generator, mlib_duration(100, ms)),
                                   mlib_duration(400, ms));

      _mongoc_retry_backoff_generator_destroy(generator);
      _mongoc_jitter_source_destroy(jitter_source);
   }
}

void
test_retry_backoff_generator_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/retry_backoff_generator", test_retry_backoff_generator);
}
