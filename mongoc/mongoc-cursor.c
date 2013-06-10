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

   return cursor;
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   bson_return_if_fail(cursor);

   bson_free(cursor);
}
