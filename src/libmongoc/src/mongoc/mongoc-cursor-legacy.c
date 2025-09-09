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

static bool
_mongoc_cursor_monitor_legacy_get_more(mongoc_cursor_t *cursor, mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   char *db;
   mongoc_client_t *client;
   mongoc_apm_command_started_t event;

   ENTRY;

   client = cursor->client;
   _mongoc_cursor_prepare_getmore_command(cursor, &doc);

   const mongoc_log_and_monitor_instance_t *log_and_monitor = &client->topology->log_and_monitor;

   mongoc_structured_log(
      log_and_monitor->structured_log,
      MONGOC_STRUCTURED_LOG_LEVEL_DEBUG,
      MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      "Command started",
      int32("requestId", client->cluster.request_id),
      server_description(server_stream->sd, SERVER_HOST, SERVER_PORT, SERVER_CONNECTION_ID, SERVICE_ID),
      utf8_n("databaseName", cursor->ns, cursor->dblen),
      utf8("commandName", "getMore"),
      int64("operationId", cursor->operation_id),
      bson_as_json("command", &doc));

   if (!log_and_monitor->apm_callbacks.started) {
      /* successful */
      bson_destroy(&doc);
      RETURN(true);
   }

   db = bson_strndup(cursor->ns, cursor->dblen);
   mongoc_apm_command_started_init(&event,
                                   &doc,
                                   db,
                                   "getMore",
                                   client->cluster.request_id,
                                   cursor->operation_id,
                                   &server_stream->sd->host,
                                   server_stream->sd->id,
                                   &server_stream->sd->service_id,
                                   server_stream->sd->server_connection_id,
                                   NULL,
                                   log_and_monitor->apm_context);

   log_and_monitor->apm_callbacks.started(&event);
   mongoc_apm_command_started_cleanup(&event);
   bson_destroy(&doc);
   bson_free(db);

   RETURN(true);
}


static bool
_mongoc_cursor_monitor_legacy_query(mongoc_cursor_t *cursor,
                                    const bson_t *filter,
                                    mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   char *db;
   bool r;

   ENTRY;

   bson_init(&doc);
   db = bson_strndup(cursor->ns, cursor->dblen);

   /* simulate a MongoDB 3.2+ "find" command */
   _mongoc_cursor_prepare_find_command(cursor, filter, &doc);

   bsonBuildAppend(cursor->opts, insert(doc, not(key("serverId", "maxAwaitTimeMS", "sessionId"))));

   r = _mongoc_cursor_monitor_command(cursor, server_stream, &doc, "find");

   bson_destroy(&doc);
   bson_free(db);

   RETURN(r);
}


static bool
_mongoc_cursor_op_getmore_send(mongoc_cursor_t *cursor,
                               mongoc_server_stream_t *server_stream,
                               int32_t request_id,
                               int32_t flags,
                               mcd_rpc_message *rpc)
{
   BSON_ASSERT_PARAM(cursor);
   BSON_ASSERT_PARAM(server_stream);
   BSON_ASSERT_PARAM(rpc);

   const int32_t n_return = (flags & MONGOC_OP_QUERY_FLAG_TAILABLE_CURSOR) != 0 ? 0 : _mongoc_n_return(cursor);

   {
      int32_t message_length = 0;

      message_length += mcd_rpc_header_set_message_length(rpc, 0);
      message_length += mcd_rpc_header_set_request_id(rpc, request_id);
      message_length += mcd_rpc_header_set_response_to(rpc, 0);
      message_length += mcd_rpc_header_set_op_code(rpc, MONGOC_OP_CODE_GET_MORE);

      message_length += sizeof(int32_t); // ZERO
      message_length += mcd_rpc_op_get_more_set_full_collection_name(rpc, cursor->ns);
      message_length += mcd_rpc_op_get_more_set_number_to_return(rpc, n_return);
      message_length += mcd_rpc_op_get_more_set_cursor_id(rpc, cursor->cursor_id);

      mcd_rpc_message_set_length(rpc, message_length);
   }

   if (!_mongoc_cursor_monitor_legacy_get_more(cursor, server_stream)) {
      return false;
   }

   if (!mongoc_cluster_legacy_rpc_sendv_to_server(&cursor->client->cluster, rpc, server_stream, &cursor->error)) {
      return false;
   }

   return true;
}

#define OPT_CHECK(_type)                                           \
   do {                                                            \
      if (!BSON_ITER_HOLDS_##_type(&iter)) {                       \
         _mongoc_set_error(&cursor->error,                         \
                           MONGOC_ERROR_COMMAND,                   \
                           MONGOC_ERROR_COMMAND_INVALID_ARG,       \
                           "invalid option %s, should be type %s", \
                           key,                                    \
                           #_type);                                \
         return NULL;                                              \
      }                                                            \
   } while (false)


#define OPT_CHECK_INT()                                            \
   do {                                                            \
      if (!BSON_ITER_HOLDS_INT(&iter)) {                           \
         _mongoc_set_error(&cursor->error,                         \
                           MONGOC_ERROR_COMMAND,                   \
                           MONGOC_ERROR_COMMAND_INVALID_ARG,       \
                           "invalid option %s, should be integer", \
                           key);                                   \
         return NULL;                                              \
      }                                                            \
   } while (false)


#define OPT_ERR(_msg)                                                                                  \
   do {                                                                                                \
      _mongoc_set_error(&cursor->error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, _msg); \
      return NULL;                                                                                     \
   } while (false)


#define OPT_BSON_ERR(_msg)                                                                   \
   do {                                                                                      \
      _mongoc_set_error(&cursor->error, MONGOC_ERROR_BSON, MONGOC_ERROR_BSON_INVALID, _msg); \
      return NULL;                                                                           \
   } while (false)


#define OPT_FLAG(_flag)               \
   do {                               \
      OPT_CHECK(BOOL);                \
      if (bson_iter_as_bool(&iter)) { \
         *flags |= _flag;             \
      }                               \
   } while (false)


#define PUSH_DOLLAR_QUERY()                                \
   do {                                                    \
      if (!pushed_dollar_query) {                          \
         pushed_dollar_query = true;                       \
         bson_append_document(query, "$query", 6, filter); \
      }                                                    \
   } while (false)


#define OPT_SUBDOCUMENT(_opt_name, _legacy_name)                          \
   do {                                                                   \
      OPT_CHECK(DOCUMENT);                                                \
      bson_iter_document(&iter, &len, &data);                             \
      if (!bson_init_static(&subdocument, data, (size_t)len)) {           \
         OPT_BSON_ERR("Invalid '" #_opt_name "' subdocument in 'opts'."); \
      }                                                                   \
      BSON_APPEND_DOCUMENT(query, "$" #_legacy_name, &subdocument);       \
   } while (false)

#undef OPT_CHECK
#undef OPT_ERR
#undef OPT_BSON_ERR
#undef OPT_FLAG
#undef OPT_SUBDOCUMENT

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
