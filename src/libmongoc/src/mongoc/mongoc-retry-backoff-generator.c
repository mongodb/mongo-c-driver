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

#include <bson/macros.h>
#include <bson/memory.h>

#include <math.h>

// MONGOC_RETRY_BACKOFF_BASE_MIN is the smallest positive base backoff representable by mlib_duration, and therefore the
// smallest a caller can supply. It bounds the attempt counter: for any positive base backoff, backoff saturates to
// `backoff_max` at or before the attempt cap, so incrementing the counter beyond that cannot change a result.
#define MONGOC_RETRY_BACKOFF_BASE_MIN mlib_duration(1, us)
#define MONGOC_DURATION_ZERO mlib_duration()

struct _mongoc_retry_backoff_generator_t {
   int attempt;
   int attempt_cap;
   mongoc_retry_backoff_params_t params;
   mongoc_jitter_source_t *jitter_source;
};

// _compute_attempt_cap returns the lowest attempt at which a MONGOC_RETRY_BACKOFF_BASE_MIN base backoff saturates to
// `backoff_max`.
static int
_compute_attempt_cap(mongoc_retry_backoff_params_t params)
{
   return (int)ceil(log((double)mlib_microseconds_count(params.backoff_max) /
                        (double)mlib_microseconds_count(MONGOC_RETRY_BACKOFF_BASE_MIN)) /
                    log(params.growth_factor));
}

mongoc_retry_backoff_generator_t *
_mongoc_retry_backoff_generator_new(mongoc_retry_backoff_params_t params, mongoc_jitter_source_t *jitter_source)
{
   BSON_ASSERT_PARAM(jitter_source);

   // Keep the attempt cap computation well-defined: `log(growth_factor)` is zero or negative for a growth factor of 1.0
   // or less, and `log(backoff_max)` is negative infinity for a zero `backoff_max`.
   BSON_ASSERT(params.growth_factor > 1.0);
   BSON_ASSERT(mlib_duration_cmp(params.backoff_max, >, MONGOC_DURATION_ZERO));

   mongoc_retry_backoff_generator_t *const generator =
      (mongoc_retry_backoff_generator_t *)bson_malloc(sizeof(mongoc_retry_backoff_generator_t));

   *generator = (mongoc_retry_backoff_generator_t){
      .attempt = 0,
      .attempt_cap = _compute_attempt_cap(params),
      .params = params,
      .jitter_source = jitter_source,
   };

   return generator;
}

void
_mongoc_retry_backoff_generator_destroy(mongoc_retry_backoff_generator_t *generator)
{
   bson_free(generator);
}

static void
_increment_attempt(mongoc_retry_backoff_generator_t *generator)
{
   generator->attempt = BSON_MIN(generator->attempt + 1, generator->attempt_cap);
}

mlib_duration
_mongoc_retry_backoff_generator_next(mongoc_retry_backoff_generator_t *generator, mlib_duration backoff_base)
{
   BSON_ASSERT_PARAM(generator);

   BSON_ASSERT(mlib_duration_cmp(backoff_base, >, MONGOC_DURATION_ZERO));

   _increment_attempt(generator);

   const double jitter = _mongoc_jitter_source_generate(generator->jitter_source);

   BSON_ASSERT(0.0 <= jitter && jitter <= 1.0);

   const mongoc_retry_backoff_params_t *const params = &generator->params;

   // `pow` may overflow to INFINITY, so clamp to `backoff_max` before converting to a duration: casting an
   // out-of-range double to an integer type is undefined behavior.
   const double backoff_us =
      fmin((double)mlib_microseconds_count(params->backoff_max),
           (double)mlib_microseconds_count(backoff_base) * pow(params->growth_factor, (double)generator->attempt));

   return mlib_duration((mlib_duration_rep_t)round(backoff_us * jitter), us);
}

void
_mongoc_retry_backoff_generator_skip(mongoc_retry_backoff_generator_t *generator)
{
   BSON_ASSERT_PARAM(generator);

   _increment_attempt(generator);
}
