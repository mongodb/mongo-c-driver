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


#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-error.h"


mongoc_cursor_t *
mongoc_cursor_new (mongoc_client_t      *client,
                   const char           *db_and_collection,
                   mongoc_query_flags_t  flags,
                   bson_uint32_t         skip,
                   bson_uint32_t         limit,
                   bson_uint32_t         batch_size,
                   const bson_t         *query,
                   const bson_t         *fields,
                   const bson_t         *options)
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

   if (options) {
      bson_copy_to(options, &cursor->options);
   } else {
      bson_init(&cursor->options);
   }

   return cursor;
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   bson_return_if_fail(cursor);

   if (cursor->ev.reply.desc.cursor_id) {
      /*
       * TODO: Call kill cursor on the server.
       */
   }

   bson_destroy(&cursor->query);
   bson_destroy(&cursor->fields);
   bson_destroy(&cursor->options);
   mongoc_event_destroy(&cursor->ev);
   bson_error_destroy(&cursor->error);

   bson_free(cursor);
}


static bson_bool_t
mongoc_cursor_unwrap_failure (mongoc_cursor_t *cursor)
{
   bson_uint32_t code = 0;
   const bson_t *b;
   bson_iter_t iter;
   bson_bool_t eof;
   const char *msg = "Unknown query failure";

   bson_return_val_if_fail(cursor, FALSE);

   if ((cursor->ev.reply.desc.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if ((b = bson_reader_read(&cursor->ev.reply.docs_reader, &eof))) {
         if (bson_iter_init_find(&iter, b, "$err")) {
            msg = bson_iter_utf8(&iter, NULL);
         }
         if (bson_iter_init_find(&iter, b, "code")) {
            code = bson_iter_int32(&iter);
         }
      }
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "Query failure: %s",
                     msg);
      return TRUE;
   } else if ((cursor->ev.reply.desc.flags & MONGOC_REPLY_CURSOR_NOT_FOUND)) {
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
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_QUERY);
   bson_uint32_t hint;
   bson_uint32_t request_id;

   bson_return_val_if_fail(cursor, FALSE);

   /*
    * TODO: Merge options.
    */

   ev.query.flags = cursor->flags;
   ev.query.nslen = cursor->nslen;
   ev.query.ns = cursor->ns;
   ev.query.skip = cursor->skip;
   ev.query.n_return = cursor->limit;
   ev.query.query = &cursor->query;
   ev.query.fields = &cursor->fields;

   if (!(hint = mongoc_client_send(cursor->client, &ev, 1, 0, &cursor->error))) {
      goto failure;
   }

   cursor->hint = hint;
   request_id = BSON_UINT32_FROM_LE(ev.any.request_id);

   if (!mongoc_client_recv(cursor->client,
                           &cursor->ev,
                           hint,
                           &cursor->error)) {
      goto failure;
   }

   if ((cursor->ev.any.opcode != MONGOC_OPCODE_REPLY) ||
       (cursor->ev.any.response_to != request_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "A reply to an invalid request id was received.");
      goto failure;
   }

   if (mongoc_cursor_unwrap_failure(cursor)) {
      goto failure;
   }

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
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_GET_MORE);
   bson_uint64_t cursor_id;
   bson_uint32_t request_id;

   bson_return_val_if_fail(cursor, FALSE);

   if (!(cursor_id = cursor->ev.reply.desc.cursor_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "No valid cursor was provided.");
      goto failure;
   }

   ev.get_more.ns = cursor->ns;
   ev.get_more.nslen = cursor->nslen;
   ev.get_more.n_return = cursor->batch_size;
   ev.get_more.cursor_id = cursor_id;

   /*
    * TODO: Stamp protections for disconnections.
    */

   if (!mongoc_client_send(cursor->client, &ev, 1, cursor->hint, &cursor->error)) {
      cursor->done = TRUE;
      cursor->failed = TRUE;
      return FALSE;
   }

   request_id = BSON_UINT32_FROM_LE(ev.any.request_id);

   if (!mongoc_client_recv(cursor->client,
                           &cursor->ev,
                           cursor->hint,
                           &cursor->error)) {
      goto failure;
   }

   if ((cursor->ev.any.opcode != MONGOC_OPCODE_REPLY) ||
       (cursor->ev.any.response_to != request_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "A reply to an invalid request id was received.");
      goto failure;
   }

   if (mongoc_cursor_unwrap_failure(cursor)) {
      goto failure;
   }

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
   b = bson_reader_read(&cursor->ev.reply.docs_reader, &eof);
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
