/*
 * Copyright 2018-present MongoDB, Inc.
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

#ifndef JSON_TEST_OPERATIONS_H
#define JSON_TEST_OPERATIONS_H

#include "json-test.h"

typedef struct {
   uint32_t n_events;
   bson_t events;
   mongoc_uri_t *test_framework_uri;
   int64_t cursor_id;
   bool acknowledged;
   bool verbose;
} json_test_ctx_t;

void
json_test_ctx_init (json_test_ctx_t *ctx);

void
json_test_ctx_cleanup (json_test_ctx_t *ctx);

void
json_test_operation (json_test_ctx_t *ctx,
                     const bson_t *test,
                     mongoc_collection_t *collection,
                     mongoc_client_session_t *session);

#endif
