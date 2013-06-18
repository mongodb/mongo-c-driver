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


#include "mongoc-client-private.h"
#include "mongoc-collection.h"
#include "mongoc-collection-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"


mongoc_collection_t *
mongoc_collection_new (mongoc_client_t *client,
                       const char      *db,
                       const char      *collection)
{
   mongoc_collection_t *col;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);
   bson_return_val_if_fail(collection, NULL);

   col = bson_malloc0(sizeof *col);
   col->client = client;

   snprintf(col->ns, sizeof col->ns - 1, "%s.%s",
            db, collection);
   snprintf(col->db, sizeof col->db - 1, "%s", db);
   snprintf(col->collection, sizeof col->collection - 1,
            "%s", collection);

   col->ns[sizeof col->ns-1] = '\0';
   col->db[sizeof col->db-1] = '\0';
   col->collection[sizeof col->collection-1] = '\0';

   col->collectionlen = strlen(col->collection);

   return col;
}


void
mongoc_collection_destroy (mongoc_collection_t *collection)
{
   bson_return_if_fail(collection);

   bson_free(collection);
}


mongoc_cursor_t *
mongoc_collection_find (mongoc_collection_t  *collection,
                        mongoc_query_flags_t  flags,
                        bson_uint32_t         skip,
                        bson_uint32_t         limit,
                        const bson_t         *query,
                        const bson_t         *fields,
                        const bson_t         *options)
{
   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   return mongoc_cursor_new(collection->client, collection->ns, flags, skip,
                            limit, 0, query, fields, options);
}


bson_bool_t
mongoc_collection_drop (mongoc_collection_t *collection,
                        bson_error_t        *error)
{
   mongoc_cursor_t *cursor;
   const bson_t *b;
   bson_iter_t iter;
   bson_bool_t ret = FALSE;
   const char *errmsg = "unknown error";
   bson_t cmd;
   char ns[140];

   bson_return_val_if_fail(collection, FALSE);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "drop", 4, collection->collection, -1);
   snprintf(ns, sizeof ns - 1, "%s.$cmd", collection->db);
   ns[sizeof ns - 1] = '\0';
   cursor = mongoc_cursor_new(collection->client, ns, MONGOC_QUERY_NONE,
                              0, 1, 0, &cmd, NULL, NULL);
   if (mongoc_cursor_next(cursor, &b)) {
      if (bson_iter_init_find(&iter, b, "ok") &&
          BSON_ITER_HOLDS_DOUBLE(&iter) &&
          bson_iter_double(&iter) == 1.0) {
         ret = TRUE;
      } else {
         if (bson_iter_init_find(&iter, b, "errmsg") &&
             BSON_ITER_HOLDS_UTF8(&iter)) {
            errmsg = bson_iter_utf8(&iter, NULL);
         }
         bson_set_error(error,
                        MONGOC_ERROR_PROTOCOL,
                        MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                        "%s",
                        errmsg);
      }
   }
   if (mongoc_cursor_error(cursor, error)) {
      /* Do nothing */
   }
   mongoc_cursor_destroy(cursor);
   bson_destroy(&cmd);

   return ret;
}


bson_bool_t
mongoc_collection_insert (mongoc_collection_t   *collection,
                          mongoc_insert_flags_t  flags,
                          const bson_t          *document,
                          const bson_t          *options,
                          bson_error_t          *error)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_INSERT);
   bson_uint32_t hint;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(document, FALSE);

   ev.insert.flags = flags;
   ev.insert.nslen = collection->collectionlen;
   ev.insert.ns = collection->collection;
   ev.insert.docslen = 1;
   ev.insert.docs = (bson_t **)&document;

   if (!(hint = mongoc_client_send(collection->client, &ev, 1, 0, error))) {
      return FALSE;
   }

   /*
    * TODO: Check options for getlasterror. Do two events and add
    *       mongoc_event_sendv() with two events.
    */

#if 0
   if (!mongoc_client_recv(collection->client, &ev, hint, error)) {
      return FALSE;
   }
#endif

   return TRUE;
}


bson_bool_t
mongoc_collection_update (mongoc_collection_t   *collection,
                          mongoc_update_flags_t  flags,
                          const bson_t          *selector,
                          const bson_t          *update,
                          const bson_t          *options,
                          bson_error_t          *error)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_UPDATE);
   bson_uint32_t hint;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(selector, FALSE);
   bson_return_val_if_fail(update, FALSE);

   ev.update.nslen = collection->collectionlen;
   ev.update.ns = collection->collection;
   ev.update.flags = flags;
   ev.update.selector = selector;
   ev.update.update = update;

   if (!(hint = mongoc_client_send(collection->client, &ev, 1, 0, error))) {
      return FALSE;
   }

   /*
    * TODO: Check options for getlasterror. Do two events and add
    *       mongoc_event_sendv() with two events.
    */

#if 0
   if (!mongoc_client_recv(collection->client, &ev, hint, error)) {
      return FALSE;
   }
#endif

   return TRUE;
}


bson_bool_t
mongoc_collection_delete (mongoc_collection_t   *collection,
                          mongoc_delete_flags_t  flags,
                          const bson_t          *selector,
                          const bson_t          *options,
                          bson_error_t          *error)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_UPDATE);
   bson_uint32_t hint;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(selector, FALSE);

   ev.delete.nslen = collection->collectionlen;
   ev.delete.ns = collection->collection;
   ev.delete.flags = flags;
   ev.delete.selector = selector;

   if (!(hint = mongoc_client_send(collection->client, &ev, 1, 0, error))) {
      return FALSE;
   }

   /*
    * TODO: Check options for getlasterror. Do two events and add
    *       mongoc_event_sendv() with two events.
    */

#if 0
   if (!mongoc_client_recv(collection->client, &ev, hint, error)) {
      return FALSE;
   }
#endif

   return TRUE;
}
