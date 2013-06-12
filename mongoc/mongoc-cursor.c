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
                   bson_error_t    *error)
{
   mongoc_cursor_t *cursor;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(hint, NULL);

   cursor = bson_malloc0(sizeof *cursor);
   cursor->client = client;
   cursor->hint = hint;
   cursor->stamp = mongoc_client_stamp(client, hint);

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

   if (cursor->client && cursor->cursor) {
      /*
       * TODO: Call kill cursor on the server.
       */
   }

   mongoc_event_destroy(&cursor->ev);

   bson_free(cursor);
}


const bson_t *
mongoc_cursor_next (mongoc_cursor_t *cursor)
{
   const bson_t *b;
   bson_bool_t eof = FALSE;

   bson_return_val_if_fail(cursor, NULL);

   if (!cursor->done) {
      if (!(b = bson_reader_read(&cursor->ev.reply.docs_reader, &eof))) {
         if (!eof) {
            /* Parse failure. */
            return FALSE;
         } else {
            /*
             * TODO: OP_GET_MORE.
             */
         }
         cursor->done = TRUE;
      }
      return b;
   }

   return NULL;
}
