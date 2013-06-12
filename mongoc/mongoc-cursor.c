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


/**
 * mongoc_cursor_new:
 * @client: A mongoc_cursor_t.
 * @hint: A hint for the target node in client.
 * @event: (transfer full): An event to be owned by the cursor.
 *
 * Creates a new mongoc_cursor_t using the parameters provided.
 *
 * @event is owned by the resulting cursor if successful.
 *
 * Returns: A mongoc_cursor_t if successful, otherwise NULL.
 *   The cursor should be destroyed with mongoc_cursor_destroy().
 */
mongoc_cursor_t *
mongoc_cursor_new (mongoc_client_t *client,
                   bson_uint32_t    hint,
                   mongoc_event_t  *event)
{
   mongoc_cursor_t *cursor;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(hint, NULL);
   bson_return_val_if_fail(event, NULL);

   cursor = bson_malloc0(sizeof *cursor);
   cursor->client = client;
   cursor->hint = hint;
   cursor->stamp = mongoc_client_stamp(client, hint);
   cursor->event = event;

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

   if (cursor->event) {
      mongoc_event_destroy(cursor->event);
      bson_free(cursor->event);
   }

   bson_free(cursor);
}


const bson_t *
mongoc_cursor_next (mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail(cursor, NULL);

   return NULL;
}
