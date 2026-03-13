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

#include <TestSuite.h>

static void
test_token_bucket(void)
{
   mongoc_token_bucket_t *const bucket = _mongoc_token_bucket_new(2.0);

   // Depositing tokens when already at capacity should do nothing.
   _mongoc_token_bucket_deposit(bucket, 1.0);
   // Consuming more tokens than capacity should fail.
   ASSERT(!_mongoc_token_bucket_consume(bucket, 2.1));
   // Consuming all tokens should succeed.
   ASSERT(_mongoc_token_bucket_consume(bucket, 2.0));
   // Consuming tokens from an exhausted token bucket should fail.
   ASSERT(!_mongoc_token_bucket_consume(bucket, 0.1));

   // Depositing more tokens than capacity should set the number of tokens equal to the capacity.
   _mongoc_token_bucket_deposit(bucket, 3.0);
   ASSERT(!_mongoc_token_bucket_consume(bucket, 3.0));
   ASSERT(_mongoc_token_bucket_consume(bucket, 2.0));
   ASSERT(!_mongoc_token_bucket_consume(bucket, 0.1));

   // Depositing fewer tokens than capacity should only partially fill the bucket.
   _mongoc_token_bucket_deposit(bucket, 1.0);
   ASSERT(!_mongoc_token_bucket_consume(bucket, 2.0));
   ASSERT(_mongoc_token_bucket_consume(bucket, 1.0));
   ASSERT(!_mongoc_token_bucket_consume(bucket, 0.1));

   // Consuming fewer tokens than capacity should leave the bucket partially filled.
   _mongoc_token_bucket_deposit(bucket, 2.0);
   ASSERT(_mongoc_token_bucket_consume(bucket, 1.0));
   ASSERT(_mongoc_token_bucket_consume(bucket, 1.0));
   ASSERT(!_mongoc_token_bucket_consume(bucket, 0.1));

   _mongoc_token_bucket_destroy(bucket);
}

void
test_token_bucket_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/token_bucket", test_token_bucket);
}
