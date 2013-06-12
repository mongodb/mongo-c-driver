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


/**
 * mongoc_cursor_new:
 * @client: A mongoc_cursor_t.
 * @hint: A hint for the target node in client.
 * @request_id: The request_id to get a response for.
 * @error: (out): A location for an error or NULL.
 *
 * Creates a new mongoc_cursor_t using the parameters provided by recieving the
 * reply from the client.
 *
 * Returns: A mongoc_cursor_t if successful, otherwise NULL.
 *   The cursor should be destroyed with mongoc_cursor_destroy().
 */
mongoc_cursor_t *
mongoc_cursor_new (mongoc_client_t *client,
                   bson_uint32_t    hint,
                   bson_int32_t     request_id,
                   const char      *ns,
                   bson_uint32_t    nslen,
                   bson_error_t    *error)
{
   mongoc_cursor_t *cursor;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(hint, NULL);

   cursor = bson_malloc0(sizeof *cursor);
   cursor->client = client;
   cursor->hint = hint;
   cursor->stamp = mongoc_client_stamp(client, hint);
   cursor->ns = strdup(ns);
   cursor->nslen = nslen;

   if (!mongoc_client_recv(client, &cursor->ev, hint, error)) {
      bson_free(cursor);
      return NULL;
   }

   if ((cursor->ev.any.opcode != MONGOC_OPCODE_REPLY) ||
       (cursor->ev.any.response_to != request_id)) {
      bson_set_error(error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Received invalid reply from server.");
      mongoc_event_destroy(&cursor->ev);
      bson_free(cursor);
      return NULL;
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

   mongoc_event_destroy(&cursor->ev);

   /*
    * TODO: probably should just make this inline.
    */
   bson_free(cursor->ns);

   bson_free(cursor);
}


static bson_bool_t
mongoc_cursor_get_more (mongoc_cursor_t *cursor)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_GET_MORE);
   bson_uint64_t cursor_id;

   bson_return_val_if_fail(cursor, FALSE);

   if (!(cursor_id = cursor->ev.reply.desc.cursor_id)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "No valid cursor was provided.");
      cursor->done = TRUE;
      cursor->failed = TRUE;
      return FALSE;
   }

   ev.get_more.ns = cursor->ns;
   ev.get_more.nslen = cursor->nslen;
   ev.get_more.n_return = cursor->batch_size;
   ev.get_more.cursor_id = cursor_id;

   /*
    * TODO: Stamp protections for disconnections.
    */

   if (!mongoc_client_send(cursor->client,
                           &ev,
                           cursor->hint,
                           &cursor->error)) {
      cursor->done = TRUE;
      cursor->failed = TRUE;
      return FALSE;
   }

   if (!mongoc_client_recv(cursor->client,
                           &cursor->ev,
                           cursor->hint,
                           &cursor->error)) {
      cursor->done = TRUE;
      cursor->failed = TRUE;
      return FALSE;
   }

   cursor->end_of_event = FALSE;

   return TRUE;
}


const bson_error_t *
mongoc_cursor_error (mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail(cursor, NULL);
   return cursor->failed ? &cursor->error : NULL;
}


const bson_t *
mongoc_cursor_next (mongoc_cursor_t *cursor)
{
   const bson_t *b;
   bson_bool_t eof;

   bson_return_val_if_fail(cursor, NULL);

   /*
    * Short circuit if we are finished already.
    */
   if (BSON_UNLIKELY(cursor->done)) {
      return NULL;
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (BSON_UNLIKELY(cursor->end_of_event)) {
      if (!mongoc_cursor_get_more(cursor)) {
         return NULL;
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
      return NULL;
   }

   return b;
}
