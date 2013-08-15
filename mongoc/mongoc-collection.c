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
#include "mongoc-opcode.h"
#include "mongoc-write-concern-private.h"


/**
 * mongoc_collection_new:
 * @client: A mongoc_client_t.
 * @db: The name of the database.
 * @collection: The name of the collection.
 * @write_concern: An optional write concern to apply as the default.
 *
 * This is an internal function.
 *
 * This function will create a new mongoc_collection_t using @client to
 * communicate.
 *
 * @client must be valid for the life of the resulting collection.
 *
 * Returns: A new mongoc_collection_t that should be freed with
 *    mongoc_collection_destroy().
 */
mongoc_collection_t *
mongoc_collection_new (mongoc_client_t              *client,
                       const char                   *db,
                       const char                   *collection,
                       const mongoc_read_prefs_t    *read_prefs,
                       const mongoc_write_concern_t *write_concern)
{
   mongoc_collection_t *col;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);
   bson_return_val_if_fail(collection, NULL);

   col = bson_malloc0(sizeof *col);
   col->client = client;
   col->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   col->read_prefs = read_prefs ?
      mongoc_read_prefs_copy(read_prefs) :
      mongoc_read_prefs_new();

   snprintf(col->ns, sizeof col->ns - 1, "%s.%s",
            db, collection);
   snprintf(col->db, sizeof col->db - 1, "%s", db);
   snprintf(col->collection, sizeof col->collection - 1,
            "%s", collection);

   col->ns[sizeof col->ns-1] = '\0';
   col->db[sizeof col->db-1] = '\0';
   col->collection[sizeof col->collection-1] = '\0';

   col->collectionlen = strlen(col->collection);
   col->nslen = strlen(col->ns);

   mongoc_buffer_init(&col->buffer, NULL, 0, NULL);

   return col;
}


/**
 * mongoc_collection_destroy:
 * @collection: (in): A mongoc_collection_t.
 *
 * Destroyes @collection and releases any associated resources.
 */
void
mongoc_collection_destroy (mongoc_collection_t *collection)
{
   bson_return_if_fail(collection);

   mongoc_buffer_destroy(&collection->buffer);

   if (collection->read_prefs) {
      mongoc_read_prefs_destroy(collection->read_prefs);
      collection->read_prefs = NULL;
   }

   if (collection->write_concern) {
      mongoc_write_concern_destroy(collection->write_concern);
      collection->write_concern = NULL;
   }

   bson_free(collection);
}


/**
 * mongoc_collection_find:
 * @collection: (in): A mongoc_collection_t.
 * @flags: (in): A bitwise or of mongoc_query_flags_t.
 * @skip: (in): The number of documents to skip.
 * @limit: (in): The maximum number of items.
 * @query: (in): The query to locate matching documents.
 * @fields: (in) (allow-none): The fields to return, or NULL for all fields.
 * @read_prefs: (in) (allow-none): Read preferences to choose cluster node.
 *
 * Performs a query against the configured MongoDB server. If @read_prefs is
 * provided, it will be used to locate a MongoDB node in the cluster to deliver
 * the query to.
 *
 * @flags may be bitwise-or'd flags or MONGOC_QUERY_NONE.
 *
 * @skip may contain the number of documents to skip before returning the
 * matching document.
 *
 * @limit may contain the maximum number of documents that may be returned.
 *
 * This function will always return a cursor, with the exception of invalid
 * API use.
 *
 * Returns: A newly created mongoc_cursor_t that should be freed with
 *    mongoc_cursor_destroy().
 */
mongoc_cursor_t *
mongoc_collection_find (mongoc_collection_t  *collection,
                        mongoc_query_flags_t  flags,
                        bson_uint32_t         skip,
                        bson_uint32_t         limit,
                        const bson_t         *query,
                        const bson_t         *fields,
                        mongoc_read_prefs_t  *read_prefs)
{
   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return mongoc_cursor_new(collection->client, collection->ns, flags, skip,
                            limit, 0, query, fields, read_prefs);
}


/**
 * mongoc_collection_command:
 * @collection: (in): A mongoc_collection_t.
 * @flags: (in): Bitwise-or'd flags for command.
 * @skip: (in): Number of documents to skip, typically 0.
 * @n_return: (in): Number of documents to return, typically 1.
 * @query: (in): The command to execute.
 * @fields: (in) (allow-none): The fields to return, or NULL.
 * @read_prefs: (in) (allow-none): Command read preferences or NULL.
 *
 * Executes a command on a cluster node matching @read_prefs. If @read_prefs
 * is not provided, it will be run on the primary node.
 *
 * This function will always return a mongoc_cursor_t with the exception of
 * invalid API use.
 *
 * Returns: A mongoc_cursor_t that should be freed with
 *    mongoc_cursor_destroy().
 */
mongoc_cursor_t *
mongoc_collection_command (mongoc_collection_t  *collection,
                           mongoc_query_flags_t  flags,
                           bson_uint32_t         skip,
                           bson_uint32_t         n_return,
                           const bson_t         *query,
                           const bson_t         *fields,
                           mongoc_read_prefs_t  *read_prefs)
{
   char ns[MONGOC_NAMESPACE_MAX+4];

   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   snprintf(ns, sizeof ns, "%s.$cmd", collection->db);
   ns[sizeof ns - 1] = '\0';

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return mongoc_cursor_new(collection->client, ns, flags, skip,
                            n_return, 0, query, fields, read_prefs);
}


bson_bool_t
mongoc_collection_command_simple (mongoc_collection_t *collection,
                                  const bson_t        *command,
                                  mongoc_read_prefs_t *read_prefs,
                                  bson_t              *reply,
                                  bson_error_t        *error)
{
   mongoc_cursor_t *cursor;
   const bson_t *b;
   bson_bool_t ret = FALSE;
   bson_iter_t iter;
   const char *errmsg = NULL;
   int code = 0;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(command, FALSE);

   cursor = mongoc_collection_command(collection, MONGOC_QUERY_NONE, 0,
                                      1, command, NULL, read_prefs);

   if (!mongoc_cursor_next(cursor, &b)) {
      mongoc_cursor_error(cursor, error);
      mongoc_cursor_destroy(cursor);
      if (reply) {
         bson_init(reply);
      }
      return FALSE;
   }

   if (reply) {
      bson_copy_to(b, reply);
   }

   if (!bson_iter_init_find(&iter, b, "ok") || !bson_iter_as_bool(&iter)) {
      if (bson_iter_init_find(&iter, b, "code") &&
          BSON_ITER_HOLDS_INT32(&iter)) {
         code = bson_iter_int32(&iter);
      }
      if (bson_iter_init_find(&iter, b, "errmsg") &&
          BSON_ITER_HOLDS_UTF8(&iter)) {
         errmsg = bson_iter_utf8(&iter, NULL);
      }
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s", errmsg ? errmsg : "Unknown command failure");
      ret = FALSE;
   } else {
      ret = TRUE;
   }

   mongoc_cursor_destroy(cursor);

   return ret;
}


bson_int64_t
mongoc_collection_count (mongoc_collection_t  *collection,
                         mongoc_query_flags_t  flags,
                         const bson_t         *query,
                         bson_int64_t          skip,
                         bson_int64_t          limit,
                         mongoc_read_prefs_t  *read_prefs,
                         bson_error_t         *error)
{
   bson_int64_t ret = -1;
   bson_iter_t iter;
   bson_t reply;
   bson_t cmd;
   bson_t q;

   bson_return_val_if_fail(collection, -1);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "count", 5, collection->collection,
                    collection->collectionlen);
   if (query) {
      bson_append_document(&cmd, "query", 5, query);
   } else {
      bson_init(&q);
      bson_append_document(&cmd, "query", 5, &q);
      bson_destroy(&q);
   }
   if (limit) {
      bson_append_int64(&cmd, "limit", 5, limit);
   }
   if (skip) {
      bson_append_int64(&cmd, "skip", 4, skip);
   }
   if (mongoc_collection_command_simple(collection, &cmd, read_prefs,
                                        &reply, error) &&
       bson_iter_init_find(&iter, &reply, "n")) {
      ret = bson_iter_as_int64(&iter);
   }
   bson_destroy(&reply);
   bson_destroy(&cmd);

   return ret;
}


bson_bool_t
mongoc_collection_drop (mongoc_collection_t *collection,
                        bson_error_t        *error)
{
   bson_bool_t ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, FALSE);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "drop", 4, collection->collection,
                    collection->collectionlen);
   ret = mongoc_collection_command_simple(collection, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


bson_bool_t
mongoc_collection_drop_index (mongoc_collection_t *collection,
                              const char          *index_name,
                              bson_error_t        *error)
{
   bson_bool_t ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(index_name, FALSE);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "dropIndexes", 9, collection->collection,
                    collection->collectionlen);
   bson_append_utf8(&cmd, "index", 5, index_name, -1);
   ret = mongoc_collection_command_simple(collection, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


bson_bool_t
mongoc_collection_insert (mongoc_collection_t    *collection,
                          mongoc_insert_flags_t   flags,
                          const bson_t           *document,
                          mongoc_write_concern_t *write_concern,
                          bson_error_t           *error)
{
   mongoc_buffer_t buffer;
   bson_uint32_t hint;
   mongoc_rpc_t rpc;
   mongoc_rpc_t reply;
   char ns[MONGOC_NAMESPACE_MAX];

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(document, FALSE);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   /*
    * Build our insert RPC.
    */
   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 0;
   rpc.insert.response_to = -1;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags = flags;
   rpc.insert.collection = collection->ns;
   rpc.insert.documents = bson_get_data(document);
   rpc.insert.documents_len = document->len;

   snprintf(ns, sizeof ns, "%s.$cmd", collection->db);
   ns[sizeof ns - 1] = '\0';

   if (!(hint = mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                    write_concern, NULL, error))) {
      return FALSE;
   }

   if (mongoc_write_concern_has_gle(write_concern)) {
      mongoc_buffer_init(&buffer, NULL, 0, NULL);
      if (!mongoc_client_recv(collection->client, &reply, &buffer, hint, error)) {
         mongoc_buffer_destroy(&buffer);
         return FALSE;
      }
      mongoc_buffer_destroy(&buffer);
   }

   return TRUE;
}


bson_bool_t
mongoc_collection_update (mongoc_collection_t    *collection,
                          mongoc_update_flags_t   flags,
                          const bson_t           *selector,
                          const bson_t           *update,
                          mongoc_write_concern_t *write_concern,
                          bson_error_t           *error)
{
   bson_uint32_t hint;
   mongoc_rpc_t rpc;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(selector, FALSE);
   bson_return_val_if_fail(update, FALSE);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   rpc.update.msg_len = 0;
   rpc.update.request_id = 0;
   rpc.update.response_to = -1;
   rpc.update.opcode = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   rpc.update.collection = collection->ns;
   rpc.update.flags = flags;
   rpc.update.selector = bson_get_data(selector);
   rpc.update.update = bson_get_data(update);

   if (!(hint = mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                    write_concern, NULL, error))) {
      return FALSE;
   }

   if (mongoc_write_concern_has_gle(write_concern)) {
      if (!mongoc_client_recv_gle(collection->client, hint, error)) {
         return FALSE;
      }
   }

   return TRUE;
}


bson_bool_t
mongoc_collection_save (mongoc_collection_t    *collection,
                        const bson_t           *document,
                        mongoc_write_concern_t *write_concern,
                        bson_error_t           *error)
{
   bson_iter_t iter;
   bson_bool_t ret;
   bson_t selector;
   bson_t set;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(document, FALSE);

   if (!bson_iter_init_find(&iter, document, "_id")) {
      return mongoc_collection_insert(collection,
                                      MONGOC_INSERT_NONE,
                                      document,
                                      write_concern,
                                      error);
   }

   bson_init(&selector);
   bson_append_iter(&selector, NULL, 0, &iter);

   bson_init(&set);
   bson_append_document(&set, "$set", 4, document);

   ret = mongoc_collection_update(collection,
                                  MONGOC_UPDATE_NONE,
                                  &selector,
                                  &set,
                                  write_concern,
                                  error);

   bson_destroy(&set);
   bson_destroy(&selector);

   return ret;
}


bson_bool_t
mongoc_collection_delete (mongoc_collection_t    *collection,
                          mongoc_delete_flags_t   flags,
                          const bson_t           *selector,
                          mongoc_write_concern_t *write_concern,
                          bson_error_t           *error)
{
   bson_uint32_t hint;
   mongoc_rpc_t rpc;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(selector, FALSE);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 0;
   rpc.delete.response_to = -1;
   rpc.delete.opcode = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   rpc.delete.collection = collection->ns;
   rpc.delete.flags = flags;
   rpc.delete.selector = bson_get_data(selector);

   if (!(hint = mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                    write_concern, NULL, error))) {
      return FALSE;
   }

   if (mongoc_write_concern_has_gle(write_concern)) {
      if (!mongoc_client_recv_gle(collection->client, hint, error)) {
         return FALSE;
      }
   }

   return TRUE;
}


const mongoc_read_prefs_t *
mongoc_collection_get_read_prefs (const mongoc_collection_t *collection)
{
   bson_return_val_if_fail(collection, NULL);
   return collection->read_prefs;
}


void
mongoc_collection_set_read_prefs (mongoc_collection_t       *collection,
                                  const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail(collection);

   if (collection->read_prefs) {
      mongoc_read_prefs_destroy(collection->read_prefs);
      collection->read_prefs = NULL;
   }

   if (read_prefs) {
      collection->read_prefs = mongoc_read_prefs_copy(read_prefs);
   }
}


const mongoc_write_concern_t *
mongoc_collection_get_write_concern (const mongoc_collection_t *collection)
{
   bson_return_val_if_fail(collection, NULL);
   return collection->write_concern;
}


void
mongoc_collection_set_write_concern (mongoc_collection_t          *collection,
                                     const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(collection);

   if (collection->write_concern) {
      mongoc_write_concern_destroy(collection->write_concern);
      collection->write_concern = NULL;
   }

   if (write_concern) {
      collection->write_concern = mongoc_write_concern_copy(write_concern);
   }
}
