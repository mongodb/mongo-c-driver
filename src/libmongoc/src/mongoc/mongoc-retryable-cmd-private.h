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

#ifndef MONGOC_RETRYABLE_CMD_PRIVATE_H
#define MONGOC_RETRYABLE_CMD_PRIVATE_H

#include <mongoc/mongoc-jitter-source-private.h>
#include <mongoc/mongoc-server-stream-private.h>
#include <mongoc/mongoc-token-bucket-private.h>

#include <mlib/duration.h>

#define MONGOC_RETRY_TOKEN_RETURN_RATE 0.1
#define MONGOC_MAX_NUM_OVERLOAD_ATTEMPTS 5

typedef enum {
   MONGOC_RETRYABLE_CMD_TYPE_READ,
   MONGOC_RETRYABLE_CMD_TYPE_WRITE,
} mongoc_retryable_cmd_type_t;

typedef struct {
   bool (*execute)(void *context, bson_t *reply, bson_error_t *error);
   mongoc_server_description_t const *(*select_retry_server)(void *context,
                                                             mongoc_deprioritized_servers_t *deprioritized_servers,
                                                             bson_t *reply,
                                                             bson_error_t *error);
   void *context;
   bool is_always_retryable;
   mongoc_retryable_cmd_type_t type;
   mongoc_jitter_source_t *jitter_source;
   mongoc_token_bucket_t *token_bucket;
   mongoc_server_description_t const *initial_server_description;
} mongoc_retryable_cmd_t;

bool
_mongoc_execute_retryable_cmd(const mongoc_retryable_cmd_t *cmd, bson_t *reply, bson_error_t *error);

#endif
