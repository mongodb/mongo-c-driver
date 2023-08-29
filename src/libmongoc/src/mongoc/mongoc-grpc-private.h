/*
 * Copyright 2023-present MongoDB, Inc.
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

#include "mongoc-prelude.h"

#ifndef MONGOC_GRPC_PRIVATE_H
#define MONGOC_GRPC_PRIVATE_H

#include "mongoc/mongoc-iovec.h"

#include <bson/bson.h>

#include <grpc/support/time.h>
#include <grpc/grpc.h>

typedef struct _mongoc_grpc_t mongoc_grpc_t;

mongoc_grpc_t *
mongoc_grpc_new (const char *target);

void
mongoc_grpc_destroy (mongoc_grpc_t *grpc);

grpc_connectivity_state
mongoc_grpc_check_connectivity_state (mongoc_grpc_t *grpc);

void
mongoc_grpc_call_cancel (mongoc_grpc_t *grpc);

bool
mongoc_grpc_start_initial_metadata (mongoc_grpc_t *grpc, bson_error_t *error);

bool
mongoc_grpc_start_message (mongoc_grpc_t *grpc,
                           int32_t request_id,
                           uint32_t flags,
                           const bson_t *cmd,
                           int32_t compressor_id,     // -1 for no compression.
                           int32_t compression_level, // -1 for no compression.
                           bson_error_t *error);

bool
mongoc_grpc_start_message_with_payload (
   mongoc_grpc_t *grpc,
   int32_t request_id,
   uint32_t flags,
   const bson_t *cmd,
   const char *payload_identifier,
   const uint8_t *payload,
   int32_t payload_size,
   int32_t compressor_id,     // -1 for no compression.
   int32_t compression_level, // -1 for no compression.
   bson_error_t *error);

bool
mongoc_grpc_handle_events (mongoc_grpc_t *grpc,
                           gpr_timespec deadline,
                           bson_error_t *error);

bool
mongoc_grpc_event_timed_out (const mongoc_grpc_t *grpc);

void
mongoc_grpc_steal_reply (mongoc_grpc_t *grpc, bson_t *reply);

#endif /* MONGOC_GRPC_PRIVATE_H */
