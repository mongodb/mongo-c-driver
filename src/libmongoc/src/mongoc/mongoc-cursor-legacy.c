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

/* cursor functions for pre-3.2 MongoDB, including:
 * - OP_QUERY find (superseded by the find command)
 * - OP_GETMORE (superseded by the getMore command)
 * - receiving OP_REPLY documents in a stream (instead of batch)
 */

#include <common-bson-dsl-private.h>
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-counters-private.h>
#include <mongoc/mongoc-cursor-private.h>
#include <mongoc/mongoc-error-private.h>
#include <mongoc/mongoc-read-concern-private.h>
#include <mongoc/mongoc-read-prefs-private.h>
#include <mongoc/mongoc-rpc-private.h>
#include <mongoc/mongoc-structured-log-private.h>
#include <mongoc/mongoc-trace-private.h>
#include <mongoc/mongoc-util-private.h>
#include <mongoc/mongoc-write-concern-private.h>

#include <mongoc/mongoc-cursor.h>
#include <mongoc/mongoc-log.h>

void
_mongoc_cursor_response_legacy_init(mongoc_cursor_response_legacy_t *response)
{
   response->rpc = mcd_rpc_message_new();
   _mongoc_buffer_init(&response->buffer, NULL, 0, NULL, NULL);
}


void
_mongoc_cursor_response_legacy_destroy(mongoc_cursor_response_legacy_t *response)
{
   if (response->reader) {
      bson_reader_destroy(response->reader);
      response->reader = NULL;
   }
   _mongoc_buffer_destroy(&response->buffer);
   mcd_rpc_message_destroy(response->rpc);
}
