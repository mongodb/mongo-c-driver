/*
 * Copyright 2013 MongoDB, Inc.
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


#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor"


#ifdef _WIN32
# define strcasecmp _stricmp
#endif


static const char *gSecondaryOkCommands [] = {
   "aggregate",
   "authenticate",
   "collstats",
   "count",
   "dbstats",
   "distinct",
   "geonear",
   "geosearch",
   "geowalk",
   "getnonce",
   "group",
   "ismaster",
   "mapreduce",
   "parallelcollectionscan",
   "ping",
   "replsetgetstatus",
   "text",
   NULL
};


static const char *
_mongoc_cursor_get_read_mode_string (mongoc_read_mode_t mode)
{
   switch (mode) {
   case MONGOC_READ_PRIMARY:
      return "primary";
   case MONGOC_READ_PRIMARY_PREFERRED:
      return "primaryPreferred";
   case MONGOC_READ_SECONDARY:
      return "secondary";
   case MONGOC_READ_SECONDARY_PREFERRED:
      return "secondaryPreferred";
   case MONGOC_READ_NEAREST:
      return "nearest";
   default:
      return "";
   }
}

static int32_t
_mongoc_n_return (mongoc_cursor_t * cursor)
{
   /* by default, use the batch size */
   int32_t r = cursor->batch_size;

   if (cursor->is_command) {
      /* commands always have n_return of 1 */
      r = 1;
   } else if (cursor->limit) {
      /* calculate remaining */
      uint32_t remaining = cursor->limit - cursor->count;

      /* use min of batch or remaining */
      r = BSON_MIN(r, (int32_t)remaining);
   }

   return r;
}

mongoc_cursor_t *
_mongoc_cursor_new (mongoc_client_t           *client,
                    const char                *db_and_collection,
                    mongoc_query_flags_t       flags,
                    uint32_t                   skip,
                    uint32_t                   limit,
                    uint32_t                   batch_size,
                    bool                       is_command,
                    const bson_t              *query,
                    const bson_t              *fields,
                    const mongoc_read_prefs_t *read_prefs)
{
   mongoc_read_prefs_t *local_read_prefs = NULL;
   mongoc_read_mode_t mode;
   mongoc_cursor_t *cursor;
   const bson_t *tags;
   bson_iter_t iter;
   const char *key;
   const char *mode_str;
   bson_t child;
   bool found = false;
   int i;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db_and_collection);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = client->read_prefs;
   }

   cursor = bson_malloc0 (sizeof *cursor);

   /*
    * DRIVERS-63:
    *
    * If this is a command and we have read_prefs other than PRIMARY, we need to
    * set SlaveOK bit in the protocol.
    */
   if (is_command &&
       read_prefs &&
       (mongoc_read_prefs_get_mode (read_prefs) != MONGOC_READ_PRIMARY)) {
      flags |= MONGOC_QUERY_SLAVE_OK;
   }

   /*
    * CDRIVER-244:
    *
    * If this is a command, we need to verify we can send it to the location
    * specified by the read preferences. Otherwise, log a warning that we
    * are rerouting to the primary instance.
    */
   if (is_command &&
       read_prefs &&
       (mongoc_read_prefs_get_mode (read_prefs) != MONGOC_READ_PRIMARY) &&
       bson_iter_init (&iter, query) &&
       bson_iter_next (&iter) &&
       (key = bson_iter_key (&iter))) {
      for (i = 0; gSecondaryOkCommands [i]; i++) {
         if (0 == strcasecmp (key, gSecondaryOkCommands [i])) {
            found = true;
            break;
         }
      }
      if (!found) {
         cursor->redir_primary = true;
         local_read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
         read_prefs = local_read_prefs;
         MONGOC_INFO ("Database command \"%s\" rerouted to primary node", key);
      }
   }

   /*
    * Cursors execute their query lazily. This sadly means that we must copy
    * some extra data around between the bson_t structures. This should be
    * small in most cases, so it reduces to a pure memcpy. The benefit to this
    * design is simplified error handling by API consumers.
    */

   cursor->client = client;
   bson_strncpy (cursor->ns, db_and_collection, sizeof cursor->ns);
   cursor->nslen = (uint32_t)strlen(cursor->ns);
   cursor->flags = flags;
   cursor->skip = skip;
   cursor->limit = limit;
   cursor->batch_size = batch_size;
   cursor->is_command = is_command;
   cursor->has_fields = !!fields;

#define MARK_FAILED(c) \
   do { \
      bson_init (&(c)->query); \
      bson_init (&(c)->fields); \
      (c)->failed = true; \
      (c)->done = true; \
      (c)->end_of_event = true; \
      (c)->sent = true; \
   } while (0)

   /* we can't have exhaust queries with limits */
   if ((flags & MONGOC_QUERY_EXHAUST) && limit) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot specify MONGOC_QUERY_EXHAUST and set a limit.");
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /* we can't have exhaust queries with sharded clusters */
   if ((flags & MONGOC_QUERY_EXHAUST) &&
       (client->cluster.mode == MONGOC_CLUSTER_SHARDED_CLUSTER)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot specify MONGOC_QUERY_EXHAUST with sharded cluster.");
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /*
    * Check types of various optional parameters.
    */
   if (!is_command) {
      if (bson_iter_init_find (&iter, query, "$explain") &&
          !(BSON_ITER_HOLDS_BOOL (&iter) || BSON_ITER_HOLDS_INT32 (&iter))) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "$explain must be a boolean.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }

      if (bson_iter_init_find (&iter, query, "$snapshot") &&
          !BSON_ITER_HOLDS_BOOL (&iter) &&
          !BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "$snapshot must be a boolean.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   /*
    * Check if we have a mixed top-level query and dollar keys such
    * as $orderby. This is not allowed (you must use {$query:{}}.
    */
   if (bson_iter_init (&iter, query)) {
      bool found_dollar = false;
      bool found_non_dollar = false;

      while (bson_iter_next (&iter)) {
         if (bson_iter_key (&iter)[0] == '$') {
            found_dollar = true;
         } else {
            found_non_dollar = true;
         }
      }

      if (found_dollar && found_non_dollar) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "Cannot mix top-level query with dollar keys such "
                         "as $orderby. Use {$query: {},...} instead.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   if (!cursor->is_command && !bson_has_field (query, "$query")) {
      bson_init (&cursor->query);
      bson_append_document (&cursor->query, "$query", 6, query);
   } else {
      bson_copy_to (query, &cursor->query);
   }

   if (read_prefs) {
      cursor->read_prefs = mongoc_read_prefs_copy (read_prefs);

      mode = mongoc_read_prefs_get_mode (read_prefs);
      tags = mongoc_read_prefs_get_tags (read_prefs);

      if (mode != MONGOC_READ_PRIMARY) {
         flags |= MONGOC_QUERY_SLAVE_OK;

         if ((mode != MONGOC_READ_SECONDARY_PREFERRED) || tags) {
            bson_append_document_begin (&cursor->query, "$readPreference",
                                        15, &child);
            mode_str = _mongoc_cursor_get_read_mode_string (mode);
            bson_append_utf8 (&child, "mode", 4, mode_str, -1);
            if (tags) {
               bson_append_array (&child, "tags", 4, tags);
            }
            bson_append_document_end (&cursor->query, &child);
         }
      }
   }

   if (fields) {
      bson_copy_to(fields, &cursor->fields);
   } else {
      bson_init(&cursor->fields);
   }

   _mongoc_buffer_init(&cursor->buffer, NULL, 0, NULL, NULL);

finish:
   mongoc_counter_cursors_active_inc();

   if (local_read_prefs) {
      mongoc_read_prefs_destroy (local_read_prefs);
   }

   RETURN (cursor);
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   BSON_ASSERT(cursor);

   if (cursor->iface.destroy) {
      cursor->iface.destroy(cursor);
   } else {
      _mongoc_cursor_destroy(cursor);
   }

   EXIT;
}

void
_mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_return_if_fail(cursor);

   if (cursor->in_exhaust) {
      cursor->client->in_exhaust = false;

      if (!cursor->done) {
         _mongoc_cluster_disconnect_node (
            &cursor->client->cluster,
            &cursor->client->cluster.nodes[cursor->hint - 1]);
      }
   } else if (cursor->rpc.reply.cursor_id) {
      mongoc_client_kill_cursor(cursor->client, cursor->rpc.reply.cursor_id);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
      cursor->reader = NULL;
   }

   bson_destroy(&cursor->query);
   bson_destroy(&cursor->fields);
   _mongoc_buffer_destroy(&cursor->buffer);
   mongoc_read_prefs_destroy(cursor->read_prefs);

   bson_free(cursor);

   mongoc_counter_cursors_active_dec();
   mongoc_counter_cursors_disposed_inc();

   EXIT;
}


static void
_mongoc_cursor_populate_error (mongoc_cursor_t *cursor,
                               const bson_t    *doc,
                               bson_error_t    *error)
{
   uint32_t code = MONGOC_ERROR_QUERY_FAILURE;
   bson_iter_t iter;
   const char *msg = "Unknown query failure";

   BSON_ASSERT (cursor);
   BSON_ASSERT (doc);
   BSON_ASSERT (error);

   if (bson_iter_init_find (&iter, doc, "code") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      code = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, doc, "$err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   if (cursor->is_command &&
       bson_iter_init_find (&iter, doc, "errmsg") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   bson_set_error(error, MONGOC_ERROR_QUERY, code, "%s", msg);
}


static bool
_mongoc_cursor_unwrap_failure (mongoc_cursor_t *cursor)
{
   bson_iter_t iter;
   bson_t b;

   ENTRY;

   bson_return_val_if_fail(cursor, false);

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Received rpc other than OP_REPLY.");
      RETURN(true);
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if (_mongoc_rpc_reply_get_first(&cursor->rpc.reply, &b)) {
         _mongoc_cursor_populate_error(cursor, &b, &cursor->error);
         bson_destroy(&b);
      } else {
         bson_set_error(&cursor->error,
                        MONGOC_ERROR_QUERY,
                        MONGOC_ERROR_QUERY_FAILURE,
                        "Unknown query failure.");
      }
      RETURN(true);
   } else if (cursor->is_command) {
      if (_mongoc_rpc_reply_get_first (&cursor->rpc.reply, &b)) {
         if (bson_iter_init_find (&iter, &b, "ok")) {
            if (bson_iter_as_bool (&iter)) {
               RETURN (false);
            } else {
               _mongoc_cursor_populate_error (cursor, &b, &cursor->error);
               bson_destroy (&b);
               RETURN (true);
            }
         }
      } else {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Failed to decode document from the server.");
         RETURN (true);
      }
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_CURSOR_NOT_FOUND)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "The cursor is invalid or has expired.");
      RETURN(true);
   }

   RETURN(false);
}


static bool
_mongoc_cursor_query (mongoc_cursor_t *cursor)
{
   mongoc_rpc_t rpc;
   uint32_t hint;
   uint32_t request_id;

   ENTRY;

   bson_return_val_if_fail (cursor, false);

   if (!_mongoc_client_warm_up (cursor->client, &cursor->error)) {
      cursor->failed = true;
      RETURN (false);
   }

   rpc.query.msg_len = 0;
   rpc.query.request_id = 0;
   rpc.query.response_to = 0;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = cursor->flags;
   rpc.query.collection = cursor->ns;
   rpc.query.skip = cursor->skip;
   if ((cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR)) {
      rpc.query.n_return = 0;
   } else {
      rpc.query.n_return = _mongoc_n_return(cursor);
   }
   rpc.query.query = bson_get_data(&cursor->query);

   if (cursor->has_fields) {
      rpc.query.fields = bson_get_data (&cursor->fields);
   } else {
      rpc.query.fields = NULL;
   }

   if (!(hint = _mongoc_client_sendv (cursor->client, &rpc, 1,
                                      cursor->hint, NULL,
                                      cursor->read_prefs,
                                      &cursor->error))) {
      GOTO (failure);
   }

   cursor->hint = hint;
   request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);

   _mongoc_buffer_clear(&cursor->buffer, false);

   if (!_mongoc_client_recv(cursor->client,
                            &cursor->rpc,
                            &cursor->buffer,
                            hint,
                            &cursor->error)) {
      GOTO (failure);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (failure);
   }

   if (cursor->rpc.header.response_to != request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to. Expected %d, got %d.",
                      request_id, cursor->rpc.header.response_to);
      GOTO (failure);
   }

   if (_mongoc_cursor_unwrap_failure(cursor)) {
      GOTO (failure);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data(cursor->rpc.reply.documents,
                                              cursor->rpc.reply.documents_len);

   if ((cursor->flags & MONGOC_QUERY_EXHAUST)) {
      cursor->in_exhaust = true;
      cursor->client->in_exhaust = true;
   }

   cursor->done = false;
   cursor->end_of_event = false;
   cursor->sent = true;

   RETURN (true);

failure:
   cursor->failed = true;
   cursor->done = true;

   RETURN (false);
}


static bool
_mongoc_cursor_get_more (mongoc_cursor_t *cursor)
{
   uint64_t cursor_id;
   uint32_t request_id;
   mongoc_rpc_t rpc;

   ENTRY;

   BSON_ASSERT (cursor);

   if (!cursor->in_exhaust) {
      if (!_mongoc_client_warm_up (cursor->client, &cursor->error)) {
         cursor->failed = true;
         RETURN (false);
      }

      if (!(cursor_id = cursor->rpc.reply.cursor_id)) {
         bson_set_error(&cursor->error,
                        MONGOC_ERROR_CURSOR,
                        MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                        "No valid cursor was provided.");
         GOTO (failure);
      }

      rpc.get_more.msg_len = 0;
      rpc.get_more.request_id = 0;
      rpc.get_more.response_to = 0;
      rpc.get_more.opcode = MONGOC_OPCODE_GET_MORE;
      rpc.get_more.zero = 0;
      rpc.get_more.collection = cursor->ns;
      if ((cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR)) {
         rpc.get_more.n_return = 0;
      } else {
         rpc.get_more.n_return = _mongoc_n_return(cursor);
      }
      rpc.get_more.cursor_id = cursor_id;

      /*
       * TODO: Stamp protections for disconnections.
       */

      if (!_mongoc_client_sendv(cursor->client, &rpc, 1, cursor->hint,
                                NULL, cursor->read_prefs, &cursor->error)) {
         cursor->done = true;
         cursor->failed = true;
         RETURN (false);
      }

      request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);
   } else {
      request_id = BSON_UINT32_FROM_LE(cursor->rpc.header.request_id);
   }

   _mongoc_buffer_clear(&cursor->buffer, false);

   if (!_mongoc_client_recv(cursor->client,
                            &cursor->rpc,
                            &cursor->buffer,
                            cursor->hint,
                            &cursor->error)) {
      GOTO (failure);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (failure);
   }

   if (cursor->rpc.header.response_to != request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to. Expected %d, got %d.",
                      request_id, cursor->rpc.header.response_to);
      GOTO (failure);
   }

   if (_mongoc_cursor_unwrap_failure(cursor)) {
      GOTO (failure);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data(cursor->rpc.reply.documents,
                                              cursor->rpc.reply.documents_len);

   cursor->end_of_event = false;

   RETURN(true);

failure:
   cursor->done = true;
   cursor->failed = true;

   RETURN(false);
}


bool
mongoc_cursor_error (mongoc_cursor_t *cursor,
                     bson_error_t    *error)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);

   if (cursor->iface.error) {
      ret = cursor->iface.error(cursor, error);
   } else {
      ret = _mongoc_cursor_error(cursor, error);
   }

   if (ret && error) {
      /*
       * Rewrite the error code if we are talking to an older mongod
       * and the command was not found. It used to simply return an
       * error code of 17 and we can synthesize 59.
       *
       * Additionally, old versions of mongos may send 13390 indicating
       * unrecognized command.
       */
      if (cursor->is_command &&
          ((error->code == MONGOC_ERROR_PROTOCOL_ERROR) ||
           (error->code == 13390))) {
         error->code = MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND;
      }
   }

   RETURN(ret);
}


bool
_mongoc_cursor_error (mongoc_cursor_t *cursor,
                      bson_error_t    *error)
{
   ENTRY;

   bson_return_val_if_fail(cursor, false);

   if (BSON_UNLIKELY(cursor->failed)) {
      bson_set_error(error,
                     cursor->error.domain,
                     cursor->error.code,
                     "%s",
                     cursor->error.message);
      RETURN(true);
   }

   RETURN(false);
}


bool
mongoc_cursor_next (mongoc_cursor_t  *cursor,
                    const bson_t    **bson)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);
   BSON_ASSERT(bson);

   TRACE ("cursor_id(%"PRId64")", cursor->rpc.reply.cursor_id);

   if (bson) {
      *bson = NULL;
   }

   if (cursor->failed) {
      return false;
   }

   if (cursor->iface.next) {
      ret = cursor->iface.next(cursor, bson);
   } else {
      ret = _mongoc_cursor_next(cursor, bson);
   }

   cursor->current = *bson;

   cursor->count++;

   RETURN(ret);
}


bool
_mongoc_cursor_next (mongoc_cursor_t  *cursor,
                     const bson_t    **bson)
{
   const bson_t *b;
   bool eof;

   ENTRY;

   BSON_ASSERT (cursor);

   if (bson) {
      *bson = NULL;
   }

   if (cursor->done || cursor->failed) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot advance a completed or failed cursor.");
      RETURN (false);
   }

   /*
    * We cannot proceed if another cursor is receiving results in exhaust mode.
    */
   if (cursor->client->in_exhaust && !cursor->in_exhaust) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_IN_EXHAUST,
                      "Another cursor derived from this client is in exhaust.");
      cursor->failed = true;
      RETURN (false);
   }

   /*
    * If we reached our limit, make sure we mark this as done and do not try to
    * make further progress.
    */
   if (cursor->limit && cursor->count >= cursor->limit) {
      cursor->done = true;
      RETURN (false);
   }

   /*
    * Try to read the next document from the reader if it exists, we might
    * get NULL back and EOF, in which case we need to submit a getmore.
    */
   if (cursor->reader) {
      eof = false;
      b = bson_reader_read (cursor->reader, &eof);
      cursor->end_of_event = eof;
      if (b) {
         GOTO (complete);
      }
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (!cursor->sent) {
      if (!_mongoc_cursor_query (cursor)) {
         RETURN (false);
      }
   } else if (BSON_UNLIKELY (cursor->end_of_event) && cursor->rpc.reply.cursor_id) {
      if (!_mongoc_cursor_get_more (cursor)) {
         RETURN (false);
      }
   }

   eof = false;
   b = bson_reader_read (cursor->reader, &eof);
   cursor->end_of_event = eof;

complete:
   cursor->done = (cursor->end_of_event &&
                   ((cursor->in_exhaust && !cursor->rpc.reply.cursor_id) ||
                    (!b && !(cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR))));

   /*
    * Do a supplimental check to see if we had a corrupted reply in the
    * document stream.
    */
   if (!b && !eof) {
      cursor->failed = true;
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "The reply was corrupt.");
      RETURN (false);
   }

   if (bson) {
      *bson = b;
   }

   RETURN (!!b);
}


bool
mongoc_cursor_more (mongoc_cursor_t *cursor)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);

   if (cursor->iface.more) {
      ret = cursor->iface.more(cursor);
   } else {
      ret = _mongoc_cursor_more(cursor);
   }

   RETURN(ret);
}


bool
_mongoc_cursor_more (mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail(cursor, false);

   if (cursor->failed) {
      return false;
   }

   return (!cursor->sent ||
           cursor->rpc.reply.cursor_id ||
           !cursor->end_of_event);
}


void
mongoc_cursor_get_host (mongoc_cursor_t    *cursor,
                        mongoc_host_list_t *host)
{
   BSON_ASSERT(cursor);
   BSON_ASSERT(host);

   if (cursor->iface.get_host) {
      cursor->iface.get_host(cursor, host);
   } else {
      _mongoc_cursor_get_host(cursor, host);
   }

   EXIT;
}


void
_mongoc_cursor_get_host (mongoc_cursor_t    *cursor,
                         mongoc_host_list_t *host)
{
   bson_return_if_fail(cursor);
   bson_return_if_fail(host);

   memset(host, 0, sizeof *host);

   if (!cursor->hint) {
      MONGOC_WARNING("%s(): Must send query before fetching peer.",
                     __FUNCTION__);
      return;
   }

   *host = cursor->client->cluster.nodes[cursor->hint - 1].host;
   host->next = NULL;
}


mongoc_cursor_t *
mongoc_cursor_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *ret;

   BSON_ASSERT(cursor);

   if (cursor->iface.clone) {
      ret = cursor->iface.clone(cursor);
   } else {
      ret = _mongoc_cursor_clone(cursor);
   }

   RETURN(ret);
}


mongoc_cursor_t *
_mongoc_cursor_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *_clone;

   ENTRY;

   BSON_ASSERT (cursor);

   _clone = bson_malloc0 (sizeof *_clone);

   _clone->client = cursor->client;
   _clone->is_command = cursor->is_command;
   _clone->flags = cursor->flags;
   _clone->skip = cursor->skip;
   _clone->batch_size = cursor->batch_size;
   _clone->limit = cursor->limit;
   _clone->nslen = cursor->nslen;
   _clone->has_fields = cursor->has_fields;

   if (cursor->read_prefs) {
      _clone->read_prefs = mongoc_read_prefs_copy (cursor->read_prefs);
   }

   bson_copy_to (&cursor->query, &_clone->query);
   bson_copy_to (&cursor->fields, &_clone->fields);

   bson_strncpy (_clone->ns, cursor->ns, sizeof _clone->ns);

   _mongoc_buffer_init (&_clone->buffer, NULL, 0, NULL, NULL);

   mongoc_counter_cursors_active_inc ();

   RETURN (_clone);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cursor_is_alive --
 *
 *       Checks to see if a cursor is alive.
 *
 *       This is primarily useful with tailable cursors.
 *
 * Returns:
 *       true if the cursor is alive.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cursor_is_alive (const mongoc_cursor_t *cursor) /* IN */
{
   bson_return_val_if_fail (cursor, false);

   return (!cursor->sent ||
           (!cursor->failed &&
            !cursor->done &&
            (cursor->rpc.header.opcode == MONGOC_OPCODE_REPLY) &&
            cursor->rpc.reply.cursor_id));
}


const bson_t *
mongoc_cursor_current (const mongoc_cursor_t *cursor) /* IN */
{
   bson_return_val_if_fail (cursor, NULL);

   return cursor->current;
}


void
mongoc_cursor_set_batch_size (mongoc_cursor_t *cursor,
                              uint32_t         batch_size)
{
   bson_return_if_fail (cursor);
   cursor->batch_size = batch_size;
}

uint32_t
mongoc_cursor_get_batch_size (const mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail (cursor, 0);

   return cursor->batch_size;
}

uint32_t
mongoc_cursor_get_hint (const mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail (cursor, 0);

   return cursor->hint;
}

int64_t
mongoc_cursor_get_id (const mongoc_cursor_t  *cursor)
{
   BSON_ASSERT(cursor);

   return cursor->rpc.reply.cursor_id;
}
