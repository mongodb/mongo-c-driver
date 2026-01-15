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

#include <mongoc/mongoc-jitter-source-private.h>
#include <mongoc/mongoc-util-private.h>

#include <math.h>


struct _mongoc_jitter_source_t {
   mongoc_jitter_source_generate_fn_t generate;
};

mongoc_jitter_source_t *
_mongoc_jitter_source_new(mongoc_jitter_source_generate_fn_t generate)
{
   mongoc_jitter_source_t *const source = (mongoc_jitter_source_t *)bson_malloc(sizeof(mongoc_jitter_source_t));

   *source = (mongoc_jitter_source_t){
      .generate = generate,
   };

   return source;
}

void
_mongoc_jitter_source_destroy(mongoc_jitter_source_t *source)
{
   bson_free(source);
}

double
_mongoc_jitter_source_generate(mongoc_jitter_source_t *source)
{
   BSON_ASSERT_PARAM(source);
   BSON_ASSERT(source->generate);

   return source->generate(source);
}

double
_mongoc_jitter_source_generate_default(mongoc_jitter_source_t *source)
{
   BSON_UNUSED(source);

   return (double)_mongoc_simple_rand_uint64_t() / (double)UINT64_MAX;
}

static mlib_duration
_duration_double_multiply(mlib_duration duration, double factor)
{
   return mlib_duration((mlib_duration_rep_t)round((double)mlib_microseconds_count(duration) * factor), us);
}

mlib_duration
_mongoc_compute_backoff_duration(double jitter, int transaction_attempt)
{
   BSON_ASSERT(0.0 <= jitter && jitter <= 1.0);
   BSON_ASSERT(transaction_attempt > 0);

   if (transaction_attempt >= MONGOC_BACKOFF_ATTEMPT_LIMIT) {
      return _duration_double_multiply(MONGOC_BACKOFF_MAX, jitter);
   }

   const double backoff_factor = pow(1.5, (double)(transaction_attempt - 1));

   return _duration_double_multiply(MONGOC_BACKOFF_INITIAL, jitter * backoff_factor);
}
