/*
 * Copyright 2013 10gen Inc.
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


#define _GNU_SOURCE

#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-opcode.h"


mongoc_cursor_t *
mongoc_cursor_new (mongoc_client_t      *client,
                   const char           *db_and_collection,
                   mongoc_query_flags_t  flags,
                   bson_uint32_t         skip,
                   bson_uint32_t         limit,
                   bson_uint32_t         batch_size,
                   const bson_t         *query,
                   const bson_t         *fields,
                   mongoc_read_prefs_t  *read_prefs)
{
   mongoc_cursor_t *cursor;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db_and_collection, NULL);
   bson_return_val_if_fail(query, NULL);

   /*
    * Cursors execute their query lazily. This sadly means that we must copy
    * some extra data around between the bson_t structures. This should be
    * small in most cases those so it reduces to a pure memcpy. The benefit
    * to this design is simplified error handling by API consumers.
    */

   cursor = bson_malloc0(sizeof *cursor);
   cursor->client = client;
   strncpy(cursor->ns, db_and_collection, sizeof cursor->ns - 1);
   cursor->nslen = strlen(cursor->ns);
   cursor->flags = flags;
   cursor->skip = skip;
   cursor->limit = limit;
   cursor->batch_size = batch_size;
   bson_copy_to(query, &cursor->query);

   if (fields) {
      bson_copy_to(fields, &cursor->fields);
   } else {
      bson_init(&cursor->fields);
   }

   if (read_prefs) {
      /*
       * TODO: copy read prefs.
       */
      cursor->read_prefs = read_prefs;
   }

   mongoc_buffer_init(&cursor->buffer, NULL, 0, NULL);

   mongoc_counter_cursors_active_inc();

   return cursor;
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   bson_return_if_fail(cursor);

   if (cursor->rpc.reply.cursor_id) {
      /*
       * TODO: Call kill cursor on the server.
       */
   }

   /*
    * TODO: Clear cursor->read_prefs once copied.
    */

   bson_destroy(&cursor->query);
   bson_destroy(&cursor->fields);
   bson_reader_destroy(&cursor->reader);
   bson_error_destroy(&cursor->error);
   mongoc_buffer_destroy(&cursor->buffer);

   bson_free(cursor);

   mongoc_counter_cursors_active_dec();
   mongoc_counter_cursors_disposed_inc();
}


static bson_bool_t
mongoc_cursor_unwrap_failure (mongoc_cursor_t *cursor)
{
   bson_uint32_t code = MONGOC_ERROR_QUERY_FAILURE;
   bson_iter_t iter;
   const char *msg = "Unknown query failure";
   bson_t b;

   bson_return_val_if_fail(cursor, FALSE);

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Received rpc other than OP_REPLY.");
      return TRUE;
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if (mongoc_rpc_reply_get_first(&cursor->rpc.reply, &b)) {
         if (bson_iter_init_find(&iter, &b, "code") &&
             BSON_ITER_HOLDS_INT32(&iter)) {
            code = bson_iter_int32(&iter);
         }
         if (bson_iter_init_find(&iter, &b, "$err") &&
             BSON_ITER_HOLDS_UTF8(&iter)) {
            msg = bson_iter_utf8(&iter, NULL);
         }
         bson_destroy(&b);
      }
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s",
                     msg);
      return TRUE;
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_CURSOR_NOT_FOUND)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "The cursor is invalid or has expired.");
      return TRUE;
   }

   return FALSE;
}


static bson_bool_t
mongoc_cursor_query (mongoc_cursor_t *cursor)
{
   bson_uint32_t hint;
   bson_uint32_t request_id;
   mongoc_rpc_t rpc;

   bson_return_val_if_fail(cursor, FALSE);

   rpc.query.msg_len = 0;
   rpc.query.request_id = 0;
   rpc.query.response_to = -1;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = cursor->flags;
   rpc.query.collection = cursor->ns;
   rpc.query.skip = cursor->skip;
   rpc.query.n_return = cursor->limit;
   rpc.query.query = bson_get_data(&cursor->query);
   rpc.query.fields = bson_get_data(&cursor->fields);

   if (!(hint = mongoc_client_sendv(cursor->client, &rpc, 1, 0,
                                    NULL, cursor->read_prefs,
                                    &cursor->error))) {
      goto failure;
   }

   cursor->hint = hint;
   request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);

   mongoc_buffer_clear(&cursor->buffer, FALSE);

   if (!mongoc_client_recv(cursor->client,
                           &cursor->rpc,
                           &cursor->buffer,
                           hint,
                           &cursor->error)) {
      goto failure;
   }

   if ((cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) ||
       (cursor->rpc.header.response_to != request_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "A reply to an invalid request id was received.");
      goto failure;
   }

   if (mongoc_cursor_unwrap_failure(cursor)) {
      goto failure;
   }

   bson_reader_destroy(&cursor->reader);
   bson_reader_init_from_data(&cursor->reader,
                              cursor->rpc.reply.documents,
                              cursor->rpc.reply.documents_len);

   cursor->done = FALSE;
   cursor->end_of_event = FALSE;
   cursor->sent = TRUE;
   return TRUE;

failure:
   cursor->failed = TRUE;
   cursor->done = TRUE;
   return FALSE;
}


static bson_bool_t
mongoc_cursor_get_more (mongoc_cursor_t *cursor)
{
   bson_uint64_t cursor_id;
   bson_uint32_t request_id;
   mongoc_rpc_t rpc;

   bson_return_val_if_fail(cursor, FALSE);

   if (!(cursor_id = cursor->rpc.reply.cursor_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "No valid cursor was provided.");
      goto failure;
   }

   rpc.get_more.msg_len = 0;
   rpc.get_more.request_id = 0;
   rpc.get_more.response_to = -1;
   rpc.get_more.opcode = MONGOC_OPCODE_GET_MORE;
   rpc.get_more.zero = 0;
   rpc.get_more.collection = cursor->ns;
   rpc.get_more.n_return = cursor->batch_size;
   rpc.get_more.cursor_id = cursor_id;

   /*
    * TODO: Stamp protections for disconnections.
    */

   if (!mongoc_client_sendv(cursor->client, &rpc, 1, cursor->hint,
                            NULL, cursor->read_prefs, &cursor->error)) {
      cursor->done = TRUE;
      cursor->failed = TRUE;
      return FALSE;
   }

   mongoc_buffer_clear(&cursor->buffer, FALSE);

   request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);

   if (!mongoc_client_recv(cursor->client,
                           &cursor->rpc,
                           &cursor->buffer,
                           cursor->hint,
                           &cursor->error)) {
      goto failure;
   }

   if ((cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) ||
       (cursor->rpc.header.response_to != request_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "A reply to an invalid request id was received.");
      goto failure;
   }

   if (mongoc_cursor_unwrap_failure(cursor)) {
      goto failure;
   }

   bson_reader_destroy(&cursor->reader);
   bson_reader_init_from_data(&cursor->reader,
                              cursor->rpc.reply.documents,
                              cursor->rpc.reply.documents_len);

   cursor->end_of_event = FALSE;

   return TRUE;

failure:
   cursor->done = TRUE;
   cursor->failed = TRUE;

   return FALSE;
}


bson_bool_t
mongoc_cursor_error (mongoc_cursor_t *cursor,
                     bson_error_t    *error)
{
   bson_return_val_if_fail(cursor, FALSE);

   if (BSON_UNLIKELY(cursor->failed)) {
      bson_set_error(error,
                     cursor->error.domain,
                     cursor->error.code,
                     "%s",
                     cursor->error.message);
      return TRUE;
   }

   return FALSE;
}


bson_bool_t
mongoc_cursor_next (mongoc_cursor_t  *cursor,
                    const bson_t    **bson)
{
   const bson_t *b;
   bson_bool_t eof;

   bson_return_val_if_fail(cursor, FALSE);

   if (bson) {
      *bson = NULL;
   }

   /*
    * Short circuit if we are finished already.
    */
   if (BSON_UNLIKELY(cursor->done)) {
      return FALSE;
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (!cursor->sent) {
      if (!mongoc_cursor_query(cursor)) {
         return FALSE;
      }
   } else if (BSON_UNLIKELY(cursor->end_of_event)) {
      if (!mongoc_cursor_get_more(cursor)) {
         return FALSE;
      }
   }

   /*
    * Read the next BSON document from the event.
    */
   eof = FALSE;
   b = bson_reader_read(&cursor->reader, &eof);
   cursor->end_of_event = eof;
   cursor->done = cursor->end_of_event && !b;

   /*
    * Do a supplimental check to see if we had a corrupted reply in the
    * document stream.
    */
   if (!b && !eof) {
      cursor->failed = TRUE;
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "The reply was corrupt.");
      return FALSE;
   }

   if (bson) {
      *bson = b;
   }

   return !!b;
}
