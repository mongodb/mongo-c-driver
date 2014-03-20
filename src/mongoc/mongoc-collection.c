/*
 * Copyright 2013 MongoDB, Inc.
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
#include "mongoc-cursor-cursorid-private.h"
#include "mongoc-cursor-array-private.h"
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
_mongoc_collection_new (mongoc_client_t              *client,
                        const char                   *db,
                        const char                   *collection,
                        const mongoc_read_prefs_t    *read_prefs,
                        const mongoc_write_concern_t *write_concern)
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

   bson_snprintf (col->ns, sizeof col->ns - 1, "%s.%s",
                  db, collection);
   bson_snprintf (col->db, sizeof col->db - 1, "%s", db);
   bson_snprintf (col->collection, sizeof col->collection - 1,
                  "%s", collection);

   col->collectionlen = (uint32_t)strlen(col->collection);
   col->nslen = (uint32_t)strlen(col->ns);

   _mongoc_buffer_init(&col->buffer, NULL, 0, NULL);

   col->gle = NULL;

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

   bson_clear (&collection->gle);

   _mongoc_buffer_destroy(&collection->buffer);

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
 *       This varies it's behavior based on the wire version.  If we're on
 *       wire_version > 0, we use the new aggregate command, which returns a
 *       database cursor.  On wire_version == 0, we create synthetic cursor on
 *       top of the array returned in result.
 *
 *       This function will always return a new mongoc_cursor_t that should
 *       be freed with mongoc_cursor_destroy().
 *
 *       The cursor may fail once iterated upon, so check
 *       mongoc_cursor_error() if mongoc_cursor_next() returns false.
 *
 *       See http://docs.mongodb.org/manual/aggregation/ for more
 *       information on how to build aggregation pipelines.
 *
 * Requires:
 *       MongoDB >= 2.1.0
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
   bson_t child;
   int32_t wire_version;

   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(pipeline, NULL);

   wire_version = collection->client->cluster.wire_version;

   bson_init(&command);
   bson_append_utf8(&command, "aggregate", 9,
                    collection->collection,
                    collection->collectionlen);
   bson_append_array(&command, "pipeline", 8, pipeline);

   /* for newer version, we include a cursor subdocument */
   if (wire_version > 0) {
      bson_append_document_begin(&command, "cursor", 6, &child);
      bson_append_int32(&child, "batchSize", 9, 0);
      bson_append_document_end(&command, &child);
   }

   cursor = mongoc_collection_command(collection, flags, 0, 1, 0, &command,
                                      NULL, read_prefs);

   if (wire_version > 0) {
      /* even for newer versions, we get back a cursor document, that we have
       * to patch in */
      _mongoc_cursor_cursorid_init(cursor);
   } else {
      /* for older versions we get an array that we can create a synthetic
       * cursor on top of */
      _mongoc_cursor_array_init(cursor);
   }

   bson_destroy(&command);

   return cursor;
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
 *       @batch_size: The batch size
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
                        uint32_t              skip,       /* IN */
                        uint32_t              limit,      /* IN */
                        uint32_t              batch_size, /* IN */
                        const bson_t              *query,      /* IN */
                        const bson_t              *fields,     /* IN */
                        const mongoc_read_prefs_t *read_prefs) /* IN */
{
   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   bson_clear (&collection->gle);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return _mongoc_cursor_new(collection->client, collection->ns, flags, skip,
                             limit, batch_size, false, query, fields, read_prefs);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_command --
 *
 *       Executes a command on a cluster node matching @read_prefs. If
 *       @read_prefs is not provided, it will be run on the primary node.
 *
 *       This function will always return a mongoc_cursor_t.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: Bitwise-or'd flags for command.
 *       @skip: Number of documents to skip, typically 0.
 *       @limit : Number of documents to return
 *       @batch_size : Batch size
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
mongoc_collection_command (mongoc_collection_t       *collection,
                           mongoc_query_flags_t       flags,
                           uint32_t              skip,
                           uint32_t              limit,
                           uint32_t              batch_size,
                           const bson_t              *query,
                           const bson_t              *fields,
                           const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (collection);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   bson_clear (&collection->gle);

   return mongoc_client_command (collection->client, collection->db, flags,
                                 skip, limit, batch_size, query, fields, read_prefs);
}

bool
mongoc_collection_command_simple (mongoc_collection_t       *collection,
                                  const bson_t              *command,
                                  const mongoc_read_prefs_t *read_prefs,
                                  bson_t                    *reply,
                                  bson_error_t              *error)
{
   BSON_ASSERT (collection);
   BSON_ASSERT (command);

   bson_clear (&collection->gle);

   return mongoc_client_command_simple (collection->client, collection->db,
                                        command, read_prefs, reply, error);
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

int64_t
mongoc_collection_count (mongoc_collection_t       *collection,  /* IN */
                         mongoc_query_flags_t       flags,       /* IN */
                         const bson_t              *query,       /* IN */
                         int64_t               skip,        /* IN */
                         int64_t               limit,       /* IN */
                         const mongoc_read_prefs_t *read_prefs,  /* IN */
                         bson_error_t              *error)       /* OUT */
{
   int64_t ret = -1;
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
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_drop (mongoc_collection_t *collection, /* IN */
                        bson_error_t        *error)      /* OUT */
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, false);

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
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_drop_index (mongoc_collection_t *collection, /* IN */
                              const char          *index_name, /* IN */
                              bson_error_t        *error)      /* OUT */
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(index_name, false);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "dropIndexes", -1, collection->collection,
                    collection->collectionlen);
   bson_append_utf8(&cmd, "index", -1, index_name, -1);
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
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

char *
mongoc_collection_keys_to_index_string (const bson_t *keys)
{
   bson_string_t *s;
   bson_iter_t iter;
   int i = 0;

   BSON_ASSERT (keys);

   if (!bson_iter_init (&iter, keys)) {
      return NULL;
   }

   s = bson_string_new (NULL);

   while (bson_iter_next (&iter)) {
      bson_string_append_printf (s,
                                 (i++ ? "_%s_%d" : "%s_%d"),
                                 bson_iter_key (&iter),
                                 bson_iter_int32 (&iter));
   }

   return bson_string_free (s, false);
}

bool
mongoc_collection_ensure_index (mongoc_collection_t      *collection,
                                const bson_t             *keys,
                                const mongoc_index_opt_t *opt,
                                bson_error_t             *error)
{
   const mongoc_index_opt_t *def_opt;
   mongoc_collection_t *col;
   bool ret;
   bson_t insert;
   char *name;

   bson_return_val_if_fail (collection, false);

   /*
    * TODO: this is supposed to be cached and cheap... make it that way
    */

   def_opt = mongoc_index_opt_get_default ();
   opt = opt ? opt : def_opt;

   if (!opt->is_initialized) {
      MONGOC_WARNING("Options have not yet been initialized");
      return false;
   }

   bson_init (&insert);

   bson_append_document (&insert, "key", -1, keys);
   bson_append_utf8 (&insert, "ns", -1, collection->ns, -1);

   if (opt->background != def_opt->background) {
      bson_append_bool (&insert, "background", -1, opt->background);
   }

   if (opt->unique != def_opt->unique) {
      bson_append_bool (&insert, "unique", -1, opt->unique);
   }

   if (opt->name != def_opt->name) {
      bson_append_utf8 (&insert, "name", -1, opt->name, -1);
   } else {
      name = mongoc_collection_keys_to_index_string(keys);
      bson_append_utf8 (&insert, "name", -1, name, -1);
      bson_free (name);
   }

   if (opt->drop_dups != def_opt->drop_dups) {
      bson_append_bool (&insert, "dropDups", -1, opt->drop_dups);
   }

   if (opt->sparse != def_opt->sparse) {
      bson_append_bool (&insert, "sparse", -1, opt->sparse);
   }

   if (opt->expire_after_seconds != def_opt->expire_after_seconds) {
      bson_append_int32 (&insert,
                         "expireAfterSeconds", -1,
                         opt->expire_after_seconds);
   }

   if (opt->v != def_opt->v) {
      bson_append_int32 (&insert, "v", -1, opt->v);
   }

   if (opt->weights != def_opt->weights) {
      bson_append_document (&insert, "weights", -1, opt->weights);
   }

   if (opt->default_language != def_opt->default_language) {
      bson_append_utf8 (&insert,
                        "defaultLanguage", -1,
                        opt->default_language, -1);
   }

   if (opt->language_override != def_opt->language_override) {
      bson_append_utf8 (&insert,
                        "languageOverride", -1,
                        opt->language_override, -1);
   }

   col = mongoc_client_get_collection (collection->client, collection->db,
                                       "system.indexes");

   ret = mongoc_collection_insert (col, MONGOC_INSERT_NO_VALIDATE, &insert, NULL,
                                   error);

   mongoc_collection_destroy(col);

   bson_destroy (&insert);

   return ret;
}


static bool
_mongoc_collection_insert_bulk_raw (mongoc_collection_t          *collection,
                                    mongoc_insert_flags_t         flags,
                                    const mongoc_iovec_t         *documents,
                                    uint32_t                      n_documents,
                                    const mongoc_write_concern_t *write_concern,
                                    bson_error_t                 *error)
{
   mongoc_buffer_t buffer;
   uint32_t hint;
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

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      return false;
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

   bson_snprintf (ns, sizeof ns, "%s.$cmd", collection->db);

   if (!(hint = _mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                     write_concern, NULL, error))) {
      return false;
   }

   if (_mongoc_write_concern_has_gle (write_concern)) {
      _mongoc_buffer_init (&buffer, NULL, 0, NULL);

      if (!_mongoc_client_recv (collection->client, &reply, &buffer,
                                hint, error)) {
         _mongoc_buffer_destroy (&buffer);
         return false;
      }

      bson_init_static (&reply_bson, reply.reply.documents,
                        reply.reply.documents_len);

      collection->gle = bson_copy (&reply_bson);

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

         _mongoc_buffer_destroy (&buffer);

         return false;
      }

      _mongoc_buffer_destroy (&buffer);
   }

   return true;
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
 *       true if successful; otherwise false and @error is set.
 *
 *       If the write concern does not dictate checking the result of the
 *       insert, then true may be returned even though the document was
 *       not actually inserted on the MongoDB server or cluster.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error may be set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_insert_bulk (mongoc_collection_t           *collection,
                               mongoc_insert_flags_t          flags,
                               const bson_t                 **documents,
                               uint32_t                  n_documents,
                               const mongoc_write_concern_t  *write_concern,
                               bson_error_t                  *error)
{
   mongoc_iovec_t *iov;
   size_t i;
   size_t err_offset;
   bool r;

   ENTRY;

   BSON_ASSERT (documents);
   BSON_ASSERT (n_documents);

   bson_clear (&collection->gle);

   if (!(flags & MONGOC_INSERT_NO_VALIDATE)) {
      for (i = 0; i < n_documents; i++) {
         if (!bson_validate (documents [i],
                             (BSON_VALIDATE_UTF8 |
                              BSON_VALIDATE_UTF8_ALLOW_NULL |
                              BSON_VALIDATE_DOLLAR_KEYS |
                              BSON_VALIDATE_DOT_KEYS),
                             &err_offset)) {
            bson_set_error (error,
                            MONGOC_ERROR_BSON,
                            MONGOC_ERROR_BSON_INVALID,
                            "A document was corrupt or contained "
                            "invalid characters . or $");
            return false;
         }
      }
   } else {
      flags &= ~MONGOC_INSERT_NO_VALIDATE;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      RETURN (false);
   }

   iov = bson_malloc (sizeof (mongoc_iovec_t) * n_documents);

   for (i = 0; i < n_documents; i++) {
      iov [i].iov_base = (void *)bson_get_data (documents [i]);
      iov [i].iov_len = documents [i]->len;
   }

   r = _mongoc_collection_insert_bulk_raw (collection, flags, iov, n_documents,
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
 *       true if successful; otherwise false and @error is set.
 *
 *       If the write concern does not dictate checking the result of the
 *       insert, then true may be returned even though the document was
 *       not actually inserted on the MongoDB server or cluster.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error may be set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_insert (mongoc_collection_t          *collection,
                          mongoc_insert_flags_t         flags,
                          const bson_t                 *document,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   bool ret;
   bson_iter_t iter;
   bson_oid_t oid;
   bson_t copy = BSON_INITIALIZER;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (document, false);

   bson_clear (&collection->gle);

   if (!bson_iter_init_find (&iter, document, "_id")) {
      bson_oid_init (&oid, NULL);
      bson_append_oid (&copy, "_id", 3, &oid);
      bson_concat (&copy, document);
      document = &copy;
   }

   ret = mongoc_collection_insert_bulk (collection, flags, &document, 1,
                                        write_concern, error);

   bson_destroy (&copy);

   return ret;
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
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error is setup upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_update (mongoc_collection_t          *collection,
                          mongoc_update_flags_t         flags,
                          const bson_t                 *selector,
                          const bson_t                 *update,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   uint32_t hint;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   size_t err_offset;

   ENTRY;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(selector, false);
   bson_return_val_if_fail(update, false);

   bson_clear (&collection->gle);

   if (!(flags & MONGOC_UPDATE_NO_VALIDATE) &&
       bson_iter_init (&iter, update) &&
       bson_iter_next (&iter) &&
       (bson_iter_key (&iter) [0] != '$') &&
       !bson_validate (update,
                       (BSON_VALIDATE_UTF8 |
                        BSON_VALIDATE_UTF8_ALLOW_NULL |
                        BSON_VALIDATE_DOLLAR_KEYS |
                        BSON_VALIDATE_DOT_KEYS),
                       &err_offset)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "update document is corrupt or contains "
                      "invalid keys including $ or .");
      return false;
   } else {
      flags &= ~MONGOC_UPDATE_NO_VALIDATE;
   }

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      RETURN (false);
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

   if (!(hint = _mongoc_client_sendv (collection->client, &rpc, 1, 0,
                                      write_concern, NULL, error))) {
      RETURN(false);
   }

   if (_mongoc_write_concern_has_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (collection->client, hint,
                                    &collection->gle, error)) {
         RETURN(false);
      }
   }

   RETURN(true);
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
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_save (mongoc_collection_t          *collection,
                        const bson_t                 *document,
                        const mongoc_write_concern_t *write_concern,
                        bson_error_t                 *error)
{
   bson_iter_t iter;
   bool ret;
   bson_t selector;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(document, false);

   if (!bson_iter_init_find(&iter, document, "_id")) {
      return mongoc_collection_insert(collection,
                                      MONGOC_INSERT_NONE,
                                      document,
                                      write_concern,
                                      error);
   }

   bson_init(&selector);
   bson_append_iter(&selector, NULL, 0, &iter);

   ret = mongoc_collection_update(collection,
                                  MONGOC_UPDATE_UPSERT,
                                  &selector,
                                  document,
                                  write_concern,
                                  error);

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
 *       true if successful; otherwise false and error is set.
 *
 *       If the write concern does not dictate checking the result, this
 *       function may return true even if it failed.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error is setup upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_delete (mongoc_collection_t          *collection,
                          mongoc_delete_flags_t         flags,
                          const bson_t                 *selector,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   uint32_t hint;
   mongoc_rpc_t rpc;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(selector, false);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!_mongoc_client_warm_up (collection->client, error)) {
      return false;
   }

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 0;
   rpc.delete.response_to = 0;
   rpc.delete.opcode = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   rpc.delete.collection = collection->ns;
   rpc.delete.flags = flags;
   rpc.delete.selector = bson_get_data(selector);

   if (!(hint = _mongoc_client_sendv(collection->client, &rpc, 1, 0,
                                     write_concern, NULL, error))) {
      return false;
   }

   if (_mongoc_write_concern_has_gle(write_concern)) {
      if (!_mongoc_client_recv_gle (collection->client, hint,
                                    &collection->gle, error)) {
         return false;
      }
   }

   return true;
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
mongoc_collection_get_read_prefs (const mongoc_collection_t *collection)
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
mongoc_collection_get_write_concern (const mongoc_collection_t *collection)
{
   bson_return_val_if_fail (collection, NULL);

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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_name --
 *
 *       Returns the name of the collection, excluding the database name.
 *
 * Returns:
 *       A string which should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
mongoc_collection_get_name (mongoc_collection_t *collection)
{
   BSON_ASSERT (collection);

   return collection->collection;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_last_error --
 *
 *       Returns getLastError document, according to write_concern on last
 *       executed command for current collection instance.
 *
 * Returns:
 *       NULL or a bson_t that should not be modified or freed. This value
 *       is not guaranteed to be persistent between calls into the
 *       mongoc_collection_t instance, and therefore must be copied if
 *       you would like to keep it around.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_t *
mongoc_collection_get_last_error (const mongoc_collection_t *collection) /* IN */
{
   bson_return_val_if_fail (collection, NULL);

   return collection->gle;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_validate --
 *
 *       Helper to call the validate command on the MongoDB server to
 *       validate the collection.
 *
 *       Options may be additional options, or NULL.
 *       Currently supported options are:
 *
 *          "full": Boolean
 *
 *       If full is true, then perform a more resource intensive
 *       validation.
 *
 *       The result is stored in reply.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set if successful.
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_validate (mongoc_collection_t *collection, /* IN */
                            const bson_t        *options,    /* IN */
                            bson_t              *reply,      /* OUT */
                            bson_error_t        *error)      /* IN */
{
   bson_iter_t iter;
   bson_t cmd = BSON_INITIALIZER;
   bool ret;

   bson_return_val_if_fail (collection, false);

   if (options &&
       bson_iter_init_find (&iter, options, "full") &&
       !BSON_ITER_HOLDS_BOOL (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "'full' must be a boolean value.");
      return false;
   }

   bson_append_utf8 (&cmd, "validate", 8,
                     collection->collection,
                     collection->collectionlen);

   if (options) {
      bson_concat (&cmd, options);
   }

   ret = mongoc_collection_command_simple (collection, &cmd, NULL, reply, error);

   bson_destroy (&cmd);

   return ret;
}
