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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_new --
 *
 *       INTERNAL API
 *
 *       Create a new mongoc_collection_t structure for the given client.
 *
 *       @client must remain valid during the lifetime of this structure.
 *       @db is the db name of the collection.
 *       @collection is the name of the collection.
 *       @read_prefs is the default read preferences to apply or NULL.
 *       @write_concern is the default write concern to apply or NULL.
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_collection_t *
mongoc_collection_new (mongoc_client_t              *client,        /* IN */
                       const char                   *db,            /* IN */
                       const char                   *collection,    /* IN */
                       const mongoc_read_prefs_t    *read_prefs,    /* IN */
                       const mongoc_write_concern_t *write_concern) /* IN */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_destroy --
 *
 *       Release resources associated with @collection and frees the
 *       structure.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_destroy (mongoc_collection_t *collection) /* IN */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_find --
 *
 *       Performs a query against the configured MongoDB server. If @read_prefs
 *       is provided, it will be used to locate a MongoDB node in the cluster
 *       to deliver the query to.
 *
 *       @flags may be bitwise-or'd flags or MONGOC_QUERY_NONE.
 *
 *       @skip may contain the number of documents to skip before returning the
 *       matching document.
 *
 *       @limit may contain the maximum number of documents that may be
 *       returned.
 *
 *       This function will always return a cursor, with the exception of
 *       invalid API use.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: A bitwise or of mongoc_query_flags_t.
 *       @skip: The number of documents to skip.
 *       @limit: The maximum number of items.
 *       @query: The query to locate matching documents.
 *       @fields: The fields to return, or NULL for all fields.
 *       @read_prefs: Read preferences to choose cluster node.
 *
 * Returns:
 *       A newly allocated mongoc_cursor_t that should be freed with
 *       mongoc_cursor_destroy().
 *
 *       The client used by mongoc_collection_t must be valid for the
 *       lifetime of the resulting mongoc_cursor_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_find (mongoc_collection_t       *collection, /* IN */
                        mongoc_query_flags_t       flags,      /* IN */
                        bson_uint32_t              skip,       /* IN */
                        bson_uint32_t              limit,      /* IN */
                        const bson_t              *query,      /* IN */
                        const bson_t              *fields,     /* IN */
                        const mongoc_read_prefs_t *read_prefs) /* IN */
{
   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return mongoc_cursor_new(collection->client, collection->ns, flags, skip,
                            limit, 0, query, fields, read_prefs);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_new --
 *
 *       Executes a command on a cluster node matching @read_prefs. If
 *       @read_prefs is not provided, it will be run on the primary node.
 *
 *       This function will always return a mongoc_cursor_t with the exception
 *       of invalid API use.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: Bitwise-or'd flags for command.
 *       @skip: Number of documents to skip, typically 0.
 *       @n_return: Number of documents to return, typically 1.
 *       @query: The command to execute.
 *       @fields: The fields to return, or NULL.
 *       @read_prefs: Command read preferences or NULL.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_command (mongoc_collection_t       *collection, /* IN */
                           mongoc_query_flags_t       flags,      /* IN */
                           bson_uint32_t              skip,       /* IN */
                           bson_uint32_t              n_return,   /* IN */
                           const bson_t              *query,      /* IN */
                           const bson_t              *fields,     /* IN */
                           const mongoc_read_prefs_t *read_prefs) /* IN */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_command_simple --
 *
 *       Helper to execute a command on a collection.
 *
 *       @reply is always set even upon failure.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @command: A bson containing the command to execute.
 *       @read_prefs: The read preferences or NULL.
 *       @reply: A location to store the result document or NULL.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @reply is always set if non-NULL and should be freed with
 *       bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_command_simple (
      mongoc_collection_t       *collection, /* IN */
      const bson_t              *command,    /* IN */
      const mongoc_read_prefs_t *read_prefs, /* IN */
      bson_t                    *reply,      /* OUT */
      bson_error_t              *error)      /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_count --
 *
 *       Count the number of documents matching @query.
 *
 * Parameters:
 *       @flags: A mongoc_query_flags_t describing the query flags or 0.
 *       @query: The query to perform or NULL for {}.
 *       @skip: The $skip to perform within the query or 0.
 *       @limit: The $limit to perform within the query or 0.
 *       @read_prefs: desired read preferences or NULL.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       -1 on failure; otherwise the number of matching documents.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bson_int64_t
mongoc_collection_count (mongoc_collection_t       *collection,  /* IN */
                         mongoc_query_flags_t       flags,       /* IN */
                         const bson_t              *query,       /* IN */
                         bson_int64_t               skip,        /* IN */
                         bson_int64_t               limit,       /* IN */
                         const mongoc_read_prefs_t *read_prefs,  /* IN */
                         bson_error_t              *error)       /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_drop --
 *
 *       Request the MongoDB server drop the collection.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is set upon failure.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_drop (mongoc_collection_t *collection, /* IN */
                        bson_error_t        *error)      /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_drop_index --
 *
 *       Request the MongoDB server drop the named index.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_drop_index (mongoc_collection_t *collection, /* IN */
                              const char          *index_name, /* IN */
                              bson_error_t        *error)      /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_insert --
 *
 *       Insert a document into a MongoDB collection.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: flags for the insert or 0.
 *       @document: The document to insert.
 *       @write_concern: A write concern or NULL.
 *       @error: a location for an error or NULL.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 *       If the write concern does not dictate checking the result of the
 *       insert, then TRUE may be returned even though the document was
 *       not actually inserted on the MongoDB server or cluster.
 *
 * Side effects:
 *       @error may be set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_insert (
      mongoc_collection_t          *collection,    /* IN */
      mongoc_insert_flags_t         flags,         /* IN */
      const bson_t                 *document,      /* IN */
      const mongoc_write_concern_t *write_concern, /* IN */
      bson_error_t                 *error)         /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_update --
 *
 *       Updates one or more documents matching @selector with @update.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: The flags for the update.
 *       @selector: A bson_t containing your selector.
 *       @update: A bson_t containing your update document.
 *       @write_concern: The write concern or NULL.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_update (mongoc_collection_t          *collection,    /* IN */
                          mongoc_update_flags_t         flags,         /* IN */
                          const bson_t                 *selector,      /* IN */
                          const bson_t                 *update,        /* IN */
                          const mongoc_write_concern_t *write_concern, /* IN */
                          bson_error_t                 *error)         /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_save --
 *
 *       Save @document to @collection.
 *
 *       If the document has an _id field, it will be updated. Otherwise,
 *       the document will be inserted into the collection.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_save (mongoc_collection_t          *collection,    /* IN */
                        const bson_t                 *document,      /* IN */
                        const mongoc_write_concern_t *write_concern, /* IN */
                        bson_error_t                 *error)         /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_delete --
 *
 *       Delete one or more items from a collection. If you want to
 *       limit to a single delete, provided MONGOC_DELETE_SINGLE_REMOVE
 *       for @flags.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: the delete flags or 0.
 *       @selector: A selector of documents to delete.
 *       @write_concern: A write concern or NULL. If NULL, the default
 *                       write concern for the collection will be used.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and error is set.
 *
 *       If the write concern does not dictate checking the result, this
 *       function may return TRUE even if it failed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_delete (mongoc_collection_t          *collection,    /* IN */
                          mongoc_delete_flags_t         flags,         /* IN */
                          const bson_t                 *selector,      /* IN */
                          const mongoc_write_concern_t *write_concern, /* IN */
                          bson_error_t                 *error)         /* OUT */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_read_prefs --
 *
 *       Fetch the default read preferences for the collection.
 *
 * Returns:
 *       A mongoc_read_prefs_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_collection_get_read_prefs (
      const mongoc_collection_t *collection) /* IN */
{
   bson_return_val_if_fail(collection, NULL);
   return collection->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_set_read_prefs --
 *
 *       Sets the default read preferences for the collection instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_set_read_prefs (
      mongoc_collection_t       *collection, /* IN */
      const mongoc_read_prefs_t *read_prefs) /* IN */
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_write_concern --
 *
 *       Fetches the default write concern for the collection instance.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_collection_get_write_concern (
      const mongoc_collection_t *collection) /* IN */
{
   bson_return_val_if_fail(collection, NULL);
   return collection->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_set_write_concern --
 *
 *       Sets the default write concern for the collection instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_set_write_concern (
      mongoc_collection_t          *collection,    /* IN */
      const mongoc_write_concern_t *write_concern) /* IN */
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
