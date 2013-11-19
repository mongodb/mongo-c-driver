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


#include <stdio.h>

#include "mongoc-client-private.h"
#include "mongoc-collection.h"
#include "mongoc-collection-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-error.h"
#include "mongoc-index.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"
#include "mongoc-write-concern-private.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "collection"


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_collection_new --
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
_mongoc_collection_new (mongoc_client_t              *client,        /* IN */
                        const char                   *db,            /* IN */
                        const char                   *collection,    /* IN */
                        const mongoc_read_prefs_t    *read_prefs,    /* IN */
                        const mongoc_write_concern_t *write_concern) /* IN */
{
   mongoc_collection_t *col;

   ENTRY;

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
      mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

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

   RETURN(col);
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
   ENTRY;

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

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_aggregate --
 *
 *       Send an "aggregate" command to the MongoDB server.
 *
 *       This function REQUIRES MongoDB 2.5.0 or higher. Sadly, there is not
 *       currently a way to auto-discover this feature. If you need
 *       support for older MongoDB versions, see
 *       mongoc_collection_aggregate_legacy().
 *
 *       This function will always return a new mongoc_cursor_t that should
 *       be freed with mongoc_cursor_destroy().
 *
 *       The cursor may fail once iterated upon, so check
 *       mongoc_cursor_error() if mongoc_cursor_next() returns FALSE.
 *
 *       See http://docs.mongodb.org/manual/aggregation/ for more
 *       information on how to build aggregation pipelines.
 *
 * Requires:
 *       MongoDB >= 2.5.0
 *
 * Parameters:
 *       @flags: bitwise or of mongoc_query_flags_t or 0.
 *       @pipeline: A bson_t containing the pipeline request. @pipeline
 *                  will be sent as an array type in the request.
 *       @read_prefs: Optional read preferences for the command.
 *
 * Returns:
 *       A newly allocated mongoc_cursor_t that should be freed with
 *       mongoc_cursor_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_aggregate (mongoc_collection_t       *collection, /* IN */
                             mongoc_query_flags_t       flags,      /* IN */
                             const bson_t              *pipeline,   /* IN */
                             const mongoc_read_prefs_t *read_prefs) /* IN */
{
   mongoc_cursor_t *cursor;
   bson_t command;

   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(pipeline, NULL);

   bson_init(&command);
   bson_append_utf8(&command, "aggregate", 9,
                    collection->collection,
                    collection->collectionlen);
   bson_append_array(&command, "pipeline", 8, pipeline);
   bson_append_int32(&command, "cursor", 6, 1);
   cursor = mongoc_collection_command(collection, flags, 0, -1, &command,
                                      NULL, read_prefs);
   bson_destroy(&command);

   return cursor;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_aggregate_legacy --
 *
 *       Support for legacy MongoDB versions that do not support commands
 *       returning cursors.
 *
 *       This function is similar to mongoc_collection_aggregate()
 *       except it returns the command document containing the "result"
 *       field for all pipeline results. This means that the result
 *       is limited to 16 Mb as that is the maximum BSON size of
 *       MongoDB (unless configured higher).
 *
 * Requires:
 *       MongoDB >= 2.1.0
 *
 * See Also:
 *       mongoc_collection_aggregate()
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @reply is always set in both success and failure cases.
 *       @error is set in case of failure.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_collection_aggregate_legacy (
      mongoc_collection_t       *collection, /* IN */
      mongoc_query_flags_t       flags,      /* IN */
      const bson_t              *pipeline,   /* IN */
      const mongoc_read_prefs_t *read_prefs, /* IN */
      bson_t                    *reply,      /* OUT */
      bson_error_t              *error)      /* OUT */
{
   bson_bool_t ret;
   bson_t command;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(pipeline, FALSE);

   bson_init(&command);
   bson_append_utf8(&command, "aggregate", 9,
                    collection->collection,
                    collection->collectionlen);
   bson_append_array(&command, "pipeline", 8, pipeline);

   ret = mongoc_collection_command_simple(collection,
                                          &command,
                                          read_prefs,
                                          reply,
                                          error);

   bson_destroy(&command);

   return ret;
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
                            limit, 0, FALSE, query, fields, read_prefs);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_command --
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
                            n_return, 0, TRUE, query, fields, read_prefs);
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
 * mongoc_collection_ensure_index --
 *
 *       Request the MongoDB server create the named index.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

char *
mongoc_collection_keys_to_index_string(const bson_t *keys)
{
   bson_string_t *s;
   bson_iter_t iter;
   int i = 0;

   s = bson_string_new(NULL);

   bson_iter_init(&iter, keys);

   while(bson_iter_next(&iter)) {
      bson_string_append_printf(s, (i++ ? "_%s_%d" : "%s_%d"), bson_iter_key(&iter), bson_iter_int32(&iter));
   }

   return bson_string_free(s, 0);
}

bson_bool_t
mongoc_collection_ensure_index (mongoc_collection_t      *collection, /* IN */
                                const bson_t             *keys,  /* IN */
                                const mongoc_index_opt_t *opt,         /* IN */
                                bson_error_t             *error) /* OUT */
{
   /** TODO: this is supposed to be cached and cheap... make it that way */

   bson_bool_t ret;
   bson_t insert;
   char * name;
   mongoc_collection_t * col;

   bson_return_val_if_fail(collection, FALSE);

   opt = opt ? opt : MONGOC_DEFAULT_INDEX_OPT;
   bson_return_val_if_fail(opt->is_initialized, FALSE);

   bson_init(&insert);

   bson_append_document(&insert, "key", -1, keys);
   bson_append_utf8(&insert, "ns", -1, collection->ns, -1);

   if (opt->background != MONGOC_DEFAULT_INDEX_OPT->background)
      bson_append_bool(&insert, "background", -1, opt->background);

   if (opt->unique != MONGOC_DEFAULT_INDEX_OPT->unique)
      bson_append_bool(&insert, "unique", -1, opt->unique);

   if (opt->name != MONGOC_DEFAULT_INDEX_OPT->name) {
      bson_append_utf8(&insert, "name", -1, opt->name, -1);
   } else {
      name = mongoc_collection_keys_to_index_string(keys);
      bson_append_utf8(&insert, "name", -1, name, -1);
      free(name);
   }

   if (opt->drop_dups != MONGOC_DEFAULT_INDEX_OPT->drop_dups)
      bson_append_bool(&insert, "dropDups", -1, opt->drop_dups);

   if (opt->sparse != MONGOC_DEFAULT_INDEX_OPT->sparse)
      bson_append_bool(&insert, "sparse", -1, opt->sparse);

   if (opt->expire_after_seconds != MONGOC_DEFAULT_INDEX_OPT->expire_after_seconds)
      bson_append_int32(&insert, "expireAfterSeconds", -1, opt->expire_after_seconds);

   if (opt->v != MONGOC_DEFAULT_INDEX_OPT->v)
      bson_append_int32(&insert, "v", -1, opt->v);

   if (opt->weights != MONGOC_DEFAULT_INDEX_OPT->weights)
      bson_append_document(&insert, "weights", -1, opt->weights);

   if (opt->default_language != MONGOC_DEFAULT_INDEX_OPT->default_language)
      bson_append_utf8(&insert, "defaultLanguage", -1, opt->default_language, -1);

   if (opt->language_override != MONGOC_DEFAULT_INDEX_OPT->language_override)
      bson_append_utf8(&insert, "languageOverride", -1, opt->language_override, -1);

   col = mongoc_client_get_collection (collection->client, collection->db,
                                       "system.indexes");

   ret = mongoc_collection_insert (col, MONGOC_INSERT_NONE, &insert, NULL, error);

   mongoc_collection_destroy(col);

   bson_destroy(&insert);

   return ret;
}


static bson_bool_t
mongoc_collection_insert_bulk_raw (
   mongoc_collection_t          *collection,       /* IN */
   mongoc_insert_flags_t         flags,            /* IN */
   const struct iovec           *documents,        /* IN */
   bson_uint32_t                 n_documents,      /* IN */
   const mongoc_write_concern_t *write_concern,    /* IN */
   bson_error_t                 *error)            /* OUT */
{
   mongoc_buffer_t buffer;
   bson_uint32_t hint;
   mongoc_rpc_t rpc;
   mongoc_rpc_t reply;
   char ns[MONGOC_NAMESPACE_MAX];
   bson_t reply_bson;
   bson_iter_t reply_iter;
   int code = 0;
   const char * errmsg;

   BSON_ASSERT(collection);
   BSON_ASSERT(documents);
   BSON_ASSERT(n_documents);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      return FALSE;
   }

   /*
    * WARNING:
    *
    *    Because we do lazy connections, we potentially have a situation
    *    here for which we have not connected to a master and determined
    *    the wire versions.
    *
    *    We might need to ensure we have a connection at this point.
    */

   if (collection->client->cluster.wire_version == 0) {
      /*
       * TODO: Do old style write commands.
       */
   } else {
      /*
       * TODO: Do new style write commands.
       */
   }

   /*
    * Build our insert RPC.
    */
   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 0;
   rpc.insert.response_to = 0;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags = flags;
   rpc.insert.collection = collection->ns;
   rpc.insert.documents = documents;
   rpc.insert.n_documents = n_documents;

   snprintf(ns, sizeof ns, "%s.$cmd", collection->db);
   ns[sizeof ns - 1] = '\0';

   if (!(hint = mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                    write_concern, NULL, error))) {
      return FALSE;
   }

   if (mongoc_write_concern_has_gle (write_concern)) {
      mongoc_buffer_init (&buffer, NULL, 0, NULL);

      if (!mongoc_client_recv (collection->client, &reply, &buffer, hint, error)) {
         mongoc_buffer_destroy (&buffer);
         return FALSE;
      }

      bson_init_static (&reply_bson, reply.reply.documents,
                        reply.reply.documents_len);

      if (bson_iter_init_find (&reply_iter, &reply_bson, "err") &&
          BSON_ITER_HOLDS_UTF8 (&reply_iter)) {
         errmsg = bson_iter_utf8 (&reply_iter, NULL);

         if (bson_iter_init_find (&reply_iter, &reply_bson, "code") &&
             BSON_ITER_HOLDS_INT32 (&reply_iter)) {
            code = bson_iter_int32 (&reply_iter);
         }

         bson_set_error (error,
                         MONGOC_ERROR_INSERT,
                         code,
                         "%s", errmsg ? errmsg : "Unknown insert failure");
         return FALSE;
      }

      mongoc_buffer_destroy (&buffer);
   }

   return TRUE;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_insert_bulk --
 *
 *       Bulk insert documents into a MongoDB collection.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: flags for the insert or 0.
 *       @documents: The documents to insert.
 *       @n_documents: The number of documents to insert.
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
mongoc_collection_insert_bulk (
   mongoc_collection_t          *collection,       /* IN */
   mongoc_insert_flags_t         flags,            /* IN */
   const bson_t                **documents,        /* IN */
   bson_uint32_t                 n_documents,      /* IN */
   const mongoc_write_concern_t *write_concern,    /* IN */
   bson_error_t                 *error)            /* OUT */
{
   struct iovec *iov;
   size_t i;
   bson_bool_t r;

   ENTRY;

   BSON_ASSERT (documents);
   BSON_ASSERT (n_documents);

   if (!_mongoc_client_warm_up (collection->client, error)) {
      RETURN (FALSE);
   }

   iov = bson_malloc (sizeof (*iov) * n_documents);

   for (i = 0; i < n_documents; i++) {
      iov[i].iov_base = (void *)bson_get_data (documents[i]);
      iov[i].iov_len = documents[i]->len;
   }

   r = mongoc_collection_insert_bulk_raw (collection, flags, iov, n_documents,
                                          write_concern, error);

   bson_free (iov);

   RETURN (r);
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
   bson_return_val_if_fail (collection, FALSE);
   bson_return_val_if_fail (document, FALSE);

   return mongoc_collection_insert_bulk (collection, flags, &document, 1,
                                         write_concern, error);
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

   ENTRY;

   bson_return_val_if_fail(collection, FALSE);
   bson_return_val_if_fail(selector, FALSE);
   bson_return_val_if_fail(update, FALSE);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      RETURN (FALSE);
   }

   rpc.update.msg_len = 0;
   rpc.update.request_id = 0;
   rpc.update.response_to = 0;
   rpc.update.opcode = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   rpc.update.collection = collection->ns;
   rpc.update.flags = flags;
   rpc.update.selector = bson_get_data(selector);
   rpc.update.update = bson_get_data(update);

   if (!(hint = mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                    write_concern, NULL, error))) {
      RETURN(FALSE);
   }

   if (mongoc_write_concern_has_gle(write_concern)) {
      if (!mongoc_client_recv_gle(collection->client, hint, error)) {
         RETURN(FALSE);
      }
   }

   RETURN(TRUE);
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

   if (!_mongoc_client_warm_up (collection->client, error)) {
      return FALSE;
   }

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 0;
   rpc.delete.response_to = 0;
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
