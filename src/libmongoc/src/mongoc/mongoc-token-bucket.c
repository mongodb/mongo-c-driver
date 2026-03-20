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

#include <mongoc/mongoc-token-bucket-private.h>

#include <math.h>

mongoc_token_bucket_t *
_mongoc_token_bucket_new(double capacity)
{
   mongoc_token_bucket_t *const bucket = (mongoc_token_bucket_t *)bson_malloc(sizeof(mongoc_token_bucket_t));

   *bucket = (mongoc_token_bucket_t){
      .mutex = {0},
      .capacity = capacity,
      .tokens = capacity,
   };

   bson_mutex_init(&bucket->mutex);

   return bucket;
}

void
_mongoc_token_bucket_destroy(mongoc_token_bucket_t *bucket)
{
   if (!bucket) {
      return;
   }

   bson_mutex_destroy(&bucket->mutex);

   bson_free(bucket);
}

bool
_mongoc_token_bucket_consume(mongoc_token_bucket_t *bucket, double n)
{
   BSON_ASSERT_PARAM(bucket);

   bson_mutex_lock(&bucket->mutex);

   const bool can_consume = bucket->tokens >= n;

   if (can_consume) {
      bucket->tokens -= n;
   }

   bson_mutex_unlock(&bucket->mutex);

   return can_consume;
}

void
_mongoc_token_bucket_deposit(mongoc_token_bucket_t *bucket, double n)
{
   BSON_ASSERT_PARAM(bucket);

   bson_mutex_lock(&bucket->mutex);

   bucket->tokens = fmin(bucket->capacity, bucket->tokens + n);

   bson_mutex_unlock(&bucket->mutex);
}
