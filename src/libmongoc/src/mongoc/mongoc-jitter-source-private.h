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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_JITTER_SOURCE_PRIVATE_H
#define MONGOC_JITTER_SOURCE_PRIVATE_H

#include <mlib/duration.h>

#define MONGOC_BACKOFF_MAX mlib_duration(500, ms)
#define MONGOC_BACKOFF_INITIAL mlib_duration(5, ms)
#define MONGOC_BACKOFF_ATTEMPT_LIMIT 13 // `5 * 1.5 ^ (n - 1) >= 500` when `n >= 13`.

typedef struct _mongoc_jitter_source_t mongoc_jitter_source_t;

// A function that returns nearly-uniformly-distributed values in the range `[0.0f, 1.0f]`.
typedef float (*mongoc_jitter_source_generate_fn_t)(mongoc_jitter_source_t *);

mongoc_jitter_source_t *
_mongoc_jitter_source_new(mongoc_jitter_source_generate_fn_t generate);

void
_mongoc_jitter_source_destroy(mongoc_jitter_source_t *source);

float
_mongoc_jitter_source_generate(mongoc_jitter_source_t *source);

float
_mongoc_jitter_source_generate_default(mongoc_jitter_source_t *source);

mlib_duration
_mongoc_compute_backoff_duration(float jitter, int transaction_attempt);

#endif
