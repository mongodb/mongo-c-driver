/*
 * Copyright 2014 MongoDB, Inc.
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

#include <bson.h>

#include "mongoc-client-private.h"
#include "mongoc-error.h"
#include "mongoc-trace-private.h"
#include "mongoc-write-command-private.h"
#include "mongoc-write-command-legacy-private.h"
#include "mongoc-write-concern-private.h"
#include "mongoc-util-private.h"


/*
 * TODO:
 *
 *    - Remove error parameter to ops, favor result->error.
 */

#define WRITE_CONCERN_DOC(wc)                                                \
   (wc) ? (_mongoc_write_concern_get_bson ((mongoc_write_concern_t *) (wc))) \
        : (&gEmptyWriteConcern)

typedef void (*mongoc_write_op_t) (mongoc_write_command_t *command,
                                   mongoc_client_t *client,
                                   mongoc_server_stream_t *server_stream,
                                   const char *database,
                                   const char *collection,
                                   const mongoc_write_concern_t *write_concern,
                                   uint32_t offset,
                                   mongoc_write_result_t *result,
                                   bson_error_t *error);


static bson_t gEmptyWriteConcern = BSON_INITIALIZER;

/* indexed by MONGOC_WRITE_COMMAND_DELETE, INSERT, UPDATE */
static const char *gCommandNames[] = {"delete", "insert", "update"};
static const char *gCommandFields[] = {"deletes", "documents", "updates"};
static const uint32_t gCommandFieldLens[] = {7, 9, 7};

static mongoc_write_op_t gLegacyWriteOps[3] = {
   _mongoc_write_command_delete_legacy,
   _mongoc_write_command_insert_legacy,
   _mongoc_write_command_update_legacy};


const char *
_mongoc_command_type_to_name (int command_type)
{
   return gCommandNames[command_type];
}

const char *
_mongoc_command_type_to_field_name (int command_type)
{
   return gCommandFields[command_type];
}

void
_mongoc_write_command_insert_append (mongoc_write_command_t *command,
                                     const bson_t *document)
{
   const char *key;
   bson_iter_t iter;
   bson_oid_t oid;
   bson_t tmp;
   char keydata[16];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (document);
   BSON_ASSERT (document->len >= 5);

   key = NULL;
   bson_uint32_to_string (command->n_documents, &key, keydata, sizeof keydata);

   BSON_ASSERT (key);

   /*
    * If the document does not contain an "_id" field, we need to generate
    * a new oid for "_id".
    */
   if (!bson_iter_init_find (&iter, document, "_id")) {
      bson_init (&tmp);
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (&tmp, "_id", &oid);
      bson_concat (&tmp, document);
      BSON_APPEND_DOCUMENT (command->documents, key, &tmp);
      bson_destroy (&tmp);
   } else {
      BSON_APPEND_DOCUMENT (command->documents, key, document);
   }

   command->n_documents++;

   EXIT;
}

void
_mongoc_write_command_update_append (mongoc_write_command_t *command,
                                     const bson_t *selector,
                                     const bson_t *update,
                                     const bson_t *opts)
{
   const char *key;
   char keydata[16];
   bson_t doc;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_UPDATE);
   BSON_ASSERT (selector && update);

   bson_init (&doc);
   BSON_APPEND_DOCUMENT (&doc, "q", selector);
   BSON_APPEND_DOCUMENT (&doc, "u", update);
   if (opts) {
      bson_concat (&doc, opts);
      command->flags.has_collation |= bson_has_field (opts, "collation");
   }

   key = NULL;
   bson_uint32_to_string (command->n_documents, &key, keydata, sizeof keydata);
   BSON_ASSERT (key);
   BSON_APPEND_DOCUMENT (command->documents, key, &doc);
   command->n_documents++;

   bson_destroy (&doc);

   EXIT;
}

void
_mongoc_write_command_delete_append (mongoc_write_command_t *command,
                                     const bson_t *selector,
                                     const bson_t *opts)
{
   const char *key;
   char keydata[16];
   bson_t doc;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_DELETE);
   BSON_ASSERT (selector);

   BSON_ASSERT (selector->len >= 5);

   bson_init (&doc);
   BSON_APPEND_DOCUMENT (&doc, "q", selector);
   if (opts) {
      bson_concat (&doc, opts);
      command->flags.has_collation |= bson_has_field (opts, "collation");
   }

   key = NULL;
   bson_uint32_to_string (command->n_documents, &key, keydata, sizeof keydata);
   BSON_ASSERT (key);
   BSON_APPEND_DOCUMENT (command->documents, key, &doc);
   command->n_documents++;

   bson_destroy (&doc);

   EXIT;
}

void
_mongoc_write_command_init_insert (mongoc_write_command_t *command, /* IN */
                                   const bson_t *document,          /* IN */
                                   mongoc_bulk_write_flags_t flags, /* IN */
                                   int64_t operation_id,            /* IN */
                                   bool allow_bulk_op_insert)       /* IN */
{
   ENTRY;

   BSON_ASSERT (command);

   command->type = MONGOC_WRITE_COMMAND_INSERT;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->flags = flags;
   command->u.insert.allow_bulk_op_insert = (uint8_t) allow_bulk_op_insert;
   command->operation_id = operation_id;

   /* must handle NULL document from mongoc_collection_insert_bulk */
   if (document) {
      _mongoc_write_command_insert_append (command, document);
   }

   EXIT;
}


void
_mongoc_write_command_init_delete (mongoc_write_command_t *command, /* IN */
                                   const bson_t *selector,          /* IN */
                                   const bson_t *opts,              /* IN */
                                   mongoc_bulk_write_flags_t flags, /* IN */
                                   int64_t operation_id)            /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);

   command->type = MONGOC_WRITE_COMMAND_DELETE;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->flags = flags;
   command->operation_id = operation_id;

   _mongoc_write_command_delete_append (command, selector, opts);

   EXIT;
}


void
_mongoc_write_command_init_update (mongoc_write_command_t *command, /* IN */
                                   const bson_t *selector,          /* IN */
                                   const bson_t *update,            /* IN */
                                   const bson_t *opts,              /* IN */
                                   mongoc_bulk_write_flags_t flags, /* IN */
                                   int64_t operation_id)            /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   command->type = MONGOC_WRITE_COMMAND_UPDATE;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->flags = flags;
   command->operation_id = operation_id;

   _mongoc_write_command_update_append (command, selector, update, opts);

   EXIT;
}


/* takes initialized bson_t *doc and begins formatting a write command */
void
_mongoc_write_command_init (bson_t *doc,
                            mongoc_write_command_t *command,
                            const char *collection,
                            const mongoc_write_concern_t *write_concern)
{
   bson_iter_t iter;

   ENTRY;

   if (!command->n_documents || !bson_iter_init (&iter, command->documents) ||
       !bson_iter_next (&iter)) {
      EXIT;
   }

   BSON_APPEND_UTF8 (doc, gCommandNames[command->type], collection);
   BSON_APPEND_DOCUMENT (
      doc, "writeConcern", WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (doc, "ordered", command->flags.ordered);

   if (command->flags.bypass_document_validation !=
       MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
      BSON_APPEND_BOOL (doc,
                        "bypassDocumentValidation",
                        !!command->flags.bypass_document_validation);
   }

   EXIT;
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_write_command_too_large_error --
 *
 *       Fill a bson_error_t and optional bson_t with error info after
 *       receiving a document for bulk insert, update, or remove that is
 *       larger than max_bson_size.
 *
 *       "err_doc" should be NULL or an empty initialized bson_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       "error" and optionally "err_doc" are filled out.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_write_command_too_large_error (bson_error_t *error,
                                       int32_t idx,
                                       int32_t len,
                                       int32_t max_bson_size,
                                       bson_t *err_doc)
{
   bson_set_error (error,
                   MONGOC_ERROR_BSON,
                   MONGOC_ERROR_BSON_INVALID,
                   "Document %u is too large for the cluster. "
                   "Document is %u bytes, max is %d.",
                   idx,
                   len,
                   max_bson_size);

   if (err_doc) {
      BSON_APPEND_INT32 (err_doc, "index", idx);
      BSON_APPEND_UTF8 (err_doc, "err", error->message);
      BSON_APPEND_INT32 (err_doc, "code", MONGOC_ERROR_BSON_INVALID);
   }
}


void
_empty_error (mongoc_write_command_t *command, bson_error_t *error)
{
   static const uint32_t codes[] = {MONGOC_ERROR_COLLECTION_DELETE_FAILED,
                                    MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                                    MONGOC_ERROR_COLLECTION_UPDATE_FAILED};

   bson_set_error (error,
                   MONGOC_ERROR_COLLECTION,
                   codes[command->type],
                   "Cannot do an empty %s",
                   gCommandNames[command->type]);
}


bool
_mongoc_write_command_will_overflow (uint32_t len_so_far,
                                     uint32_t document_len,
                                     uint32_t n_documents_written,
                                     int32_t max_bson_size,
                                     int32_t max_write_batch_size)
{
   /* max BSON object size + 16k bytes.
    * server guarantees there is enough room: SERVER-10643
    */
   int32_t max_cmd_size = max_bson_size + 16384;

   BSON_ASSERT (max_bson_size);

   if (len_so_far + document_len > max_cmd_size) {
      return true;
   } else if (max_write_batch_size > 0 &&
              n_documents_written >= max_write_batch_size) {
      return true;
   }

   return false;
}


static void
_mongoc_write_command (mongoc_write_command_t *command,
                       mongoc_client_t *client,
                       mongoc_server_stream_t *server_stream,
                       const char *database,
                       const char *collection,
                       const mongoc_write_concern_t *write_concern,
                       uint32_t offset,
                       mongoc_write_result_t *result,
                       bson_error_t *error)
{
   mongoc_cmd_parts_t parts;
   const uint8_t *data;
   bson_iter_t iter;
   const char *key;
   uint32_t len = 0;
   bson_t tmp;
   bson_t ar;
   bson_t cmd;
   bson_t reply;
   char str[16];
   bool has_more;
   bool ret = false;
   uint32_t i;
   int32_t max_bson_obj_size;
   int32_t max_write_batch_size;
   int32_t min_wire_version;
   uint32_t overhead;
   uint32_t key_len;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   bson_init (&cmd);
   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);
   max_write_batch_size =
      mongoc_server_stream_max_write_batch_size (server_stream);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */

   min_wire_version = server_stream->sd->min_wire_version;
   if ((min_wire_version == 0) &&
       !mongoc_write_concern_is_acknowledged (write_concern)) {
      if (command->flags.bypass_document_validation !=
          MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
         bson_set_error (
            error,
            MONGOC_ERROR_COMMAND,
            MONGOC_ERROR_COMMAND_INVALID_ARG,
            "Cannot set bypassDocumentValidation for unacknowledged writes");
         EXIT;
      }
      if (command->flags.has_collation) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Cannot set collation for unacknowledged writes");
         EXIT;
      }
      gLegacyWriteOps[command->type](command,
                                     client,
                                     server_stream,
                                     database,
                                     collection,
                                     write_concern,
                                     offset,
                                     result,
                                     error);
      EXIT;
   }
   if (command->flags.has_collation &&
       server_stream->sd->max_wire_version < WIRE_VERSION_COLLATION) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                      "Collation is not supported by the selected server");
      EXIT;
   }

   if (!command->n_documents || !bson_iter_init (&iter, command->documents) ||
       !bson_iter_next (&iter)) {
      _empty_error (command, error);
      result->failed = true;
      EXIT;
   }

again:
   has_more = false;
   i = 0;

   _mongoc_write_command_init (&cmd, command, collection, write_concern);

   /* 1 byte to specify array type, 1 byte for field name's null terminator */
   overhead = cmd.len + 2 + gCommandFieldLens[command->type];

   if (!_mongoc_write_command_will_overflow (overhead,
                                             command->documents->len,
                                             command->n_documents,
                                             max_bson_obj_size,
                                             max_write_batch_size)) {
      /* copy the whole documents buffer as e.g. "updates": [...] */
      bson_append_array (&cmd,
                         gCommandFields[command->type],
                         gCommandFieldLens[command->type],
                         command->documents);
      i = command->n_documents;
   } else {
      bson_append_array_begin (&cmd,
                               gCommandFields[command->type],
                               gCommandFieldLens[command->type],
                               &ar);

      do {
         BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
         bson_iter_document (&iter, &len, &data);

         /* append array element like "0": { ... doc ... } */
         key_len = (uint32_t) bson_uint32_to_string (i, &key, str, sizeof str);

         /* 1 byte to specify document type, 1 byte for key's null terminator */
         if (_mongoc_write_command_will_overflow (overhead,
                                                  key_len + len + 2 + ar.len,
                                                  i,
                                                  max_bson_obj_size,
                                                  max_write_batch_size)) {
            has_more = true;
            break;
         }

         BSON_ASSERT (bson_init_static (&tmp, data, len));
         BSON_APPEND_DOCUMENT (&ar, key, &tmp);

         bson_destroy (&tmp);

         i++;
      } while (bson_iter_next (&iter));

      bson_append_array_end (&cmd, &ar);
   }

   if (!i) {
      _mongoc_write_command_too_large_error (
         error, i, len, max_bson_obj_size, NULL);
      result->failed = true;
      ret = false;

      /* the current document is too large, continue to the next */
      if (!bson_iter_next (&iter)) {
         GOTO (cleanup);
      }
   } else {
      mongoc_cmd_parts_init (&parts, database, MONGOC_QUERY_NONE, &cmd);
      parts.is_write_command = true;
      parts.assembled.operation_id = command->operation_id;
      ret = mongoc_cluster_run_command_monitored (
         &client->cluster, &parts, server_stream, &reply, error);

      if (!ret) {
         result->failed = true;
         if (bson_empty (&reply)) {
            /* The command not only failed,
             * the roundtrip to the server failed and the node was disconnected
             */
            result->must_stop = true;
         }
      }

      _mongoc_write_result_merge (result, command, &reply, offset);
      offset += i;
      bson_destroy (&reply);
      mongoc_cmd_parts_cleanup (&parts);
   }

   if (has_more && (ret || !command->flags.ordered) && !result->must_stop) {
      bson_reinit (&cmd);
      GOTO (again);
   }

cleanup:
   bson_destroy (&cmd);
   EXIT;
}


void
_mongoc_write_command_execute (
   mongoc_write_command_t *command,             /* IN */
   mongoc_client_t *client,                     /* IN */
   mongoc_server_stream_t *server_stream,       /* IN */
   const char *database,                        /* IN */
   const char *collection,                      /* IN */
   const mongoc_write_concern_t *write_concern, /* IN */
   uint32_t offset,                             /* IN */
   mongoc_write_result_t *result)               /* OUT */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (database);
   BSON_ASSERT (collection);
   BSON_ASSERT (result);

   if (!write_concern) {
      write_concern = client->write_concern;
   }

   if (!mongoc_write_concern_is_valid (write_concern)) {
      bson_set_error (&result->error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "The write concern is invalid.");
      result->failed = true;
      EXIT;
   }

   if (server_stream->sd->max_wire_version >= WIRE_VERSION_WRITE_CMD) {
      _mongoc_write_command (command,
                             client,
                             server_stream,
                             database,
                             collection,
                             write_concern,
                             offset,
                             result,
                             &result->error);
   } else {
      if (command->flags.bypass_document_validation !=
          MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
         bson_set_error (
            &result->error,
            MONGOC_ERROR_COMMAND,
            MONGOC_ERROR_COMMAND_INVALID_ARG,
            "Cannot set bypassDocumentValidation for unacknowledged writes");
         result->failed = true;
         EXIT;
      }
      if (command->flags.has_collation &&
          server_stream->sd->max_wire_version < WIRE_VERSION_COLLATION) {
         bson_set_error (&result->error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "Cannot set collation for unacknowledged writes");
         result->failed = true;
         EXIT;
      }
      gLegacyWriteOps[command->type](command,
                                     client,
                                     server_stream,
                                     database,
                                     collection,
                                     write_concern,
                                     offset,
                                     result,
                                     &result->error);
   }

   EXIT;
}


void
_mongoc_write_command_destroy (mongoc_write_command_t *command)
{
   ENTRY;

   if (command) {
      bson_destroy (command->documents);
   }

   EXIT;
}


void
_mongoc_write_result_init (mongoc_write_result_t *result) /* IN */
{
   ENTRY;

   BSON_ASSERT (result);

   memset (result, 0, sizeof *result);

   bson_init (&result->upserted);
   bson_init (&result->writeConcernErrors);
   bson_init (&result->writeErrors);

   EXIT;
}


void
_mongoc_write_result_destroy (mongoc_write_result_t *result)
{
   ENTRY;

   BSON_ASSERT (result);

   bson_destroy (&result->upserted);
   bson_destroy (&result->writeConcernErrors);
   bson_destroy (&result->writeErrors);

   EXIT;
}


void
_mongoc_write_result_append_upsert (mongoc_write_result_t *result,
                                    int32_t idx,
                                    const bson_value_t *value)
{
   bson_t child;
   const char *keyptr = NULL;
   char key[12];
   int len;

   BSON_ASSERT (result);
   BSON_ASSERT (value);

   len = (int) bson_uint32_to_string (
      result->upsert_append_count, &keyptr, key, sizeof key);

   bson_append_document_begin (&result->upserted, keyptr, len, &child);
   BSON_APPEND_INT32 (&child, "index", idx);
   BSON_APPEND_VALUE (&child, "_id", value);
   bson_append_document_end (&result->upserted, &child);

   result->upsert_append_count++;
}


int32_t
_mongoc_write_result_merge_arrays (uint32_t offset,
                                   mongoc_write_result_t *result, /* IN */
                                   bson_t *dest,                  /* IN */
                                   bson_iter_t *iter)             /* IN */
{
   const bson_value_t *value;
   bson_iter_t ar;
   bson_iter_t citer;
   int32_t idx;
   int32_t count = 0;
   int32_t aridx;
   bson_t child;
   const char *keyptr = NULL;
   char key[12];
   int len;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (dest);
   BSON_ASSERT (iter);
   BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (iter));

   aridx = bson_count_keys (dest);

   if (bson_iter_recurse (iter, &ar)) {
      while (bson_iter_next (&ar)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
             bson_iter_recurse (&ar, &citer)) {
            len =
               (int) bson_uint32_to_string (aridx++, &keyptr, key, sizeof key);
            bson_append_document_begin (dest, keyptr, len, &child);
            while (bson_iter_next (&citer)) {
               if (BSON_ITER_IS_KEY (&citer, "index")) {
                  idx = bson_iter_int32 (&citer) + offset;
                  BSON_APPEND_INT32 (&child, "index", idx);
               } else {
                  value = bson_iter_value (&citer);
                  BSON_APPEND_VALUE (&child, bson_iter_key (&citer), value);
               }
            }
            bson_append_document_end (dest, &child);
            count++;
         }
      }
   }

   RETURN (count);
}


void
_mongoc_write_result_merge (mongoc_write_result_t *result,   /* IN */
                            mongoc_write_command_t *command, /* IN */
                            const bson_t *reply,             /* IN */
                            uint32_t offset)
{
   int32_t server_index = 0;
   const bson_value_t *value;
   bson_iter_t iter;
   bson_iter_t citer;
   bson_iter_t ar;
   int32_t n_upserted = 0;
   int32_t affected = 0;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

   if (bson_iter_init_find (&iter, reply, "n") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      affected = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeErrors") &&
       BSON_ITER_HOLDS_ARRAY (&iter) && bson_iter_recurse (&iter, &citer) &&
       bson_iter_next (&citer)) {
      result->failed = true;
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_INSERT:
      result->nInserted += affected;
      break;
   case MONGOC_WRITE_COMMAND_DELETE:
      result->nRemoved += affected;
      break;
   case MONGOC_WRITE_COMMAND_UPDATE:

      /* server returns each upserted _id with its index into this batch
       * look for "upserted": [{"index": 4, "_id": ObjectId()}, ...] */
      if (bson_iter_init_find (&iter, reply, "upserted")) {
         if (BSON_ITER_HOLDS_ARRAY (&iter) &&
             (bson_iter_recurse (&iter, &ar))) {
            while (bson_iter_next (&ar)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                   bson_iter_recurse (&ar, &citer) &&
                   bson_iter_find (&citer, "index") &&
                   BSON_ITER_HOLDS_INT32 (&citer)) {
                  server_index = bson_iter_int32 (&citer);

                  if (bson_iter_recurse (&ar, &citer) &&
                      bson_iter_find (&citer, "_id")) {
                     value = bson_iter_value (&citer);
                     _mongoc_write_result_append_upsert (
                        result, offset + server_index, value);
                     n_upserted++;
                  }
               }
            }
         }
         result->nUpserted += n_upserted;
         /*
          * XXX: The following addition to nMatched needs some checking.
          *      I'm highly skeptical of it.
          */
         result->nMatched += BSON_MAX (0, (affected - n_upserted));
      } else {
         result->nMatched += affected;
      }
      /*
       * SERVER-13001 - in a mixed sharded cluster a call to update could
       * return nModified (>= 2.6) or not (<= 2.4).  If any call does not
       * return nModified we can't report a valid final count so omit the
       * field completely.
       */
      if (bson_iter_init_find (&iter, reply, "nModified") &&
          BSON_ITER_HOLDS_INT32 (&iter)) {
         result->nModified += bson_iter_int32 (&iter);
      } else {
         /*
          * nModified could be BSON_TYPE_NULL, which should also be omitted.
          */
         result->omit_nModified = true;
      }
      break;
   default:
      BSON_ASSERT (false);
      break;
   }

   if (bson_iter_init_find (&iter, reply, "writeErrors") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      _mongoc_write_result_merge_arrays (
         offset, result, &result->writeErrors, &iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeConcernError") &&
       BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      uint32_t len;
      const uint8_t *data;
      bson_t write_concern_error;
      char str[16];
      const char *key;

      /* writeConcernError is a subdocument in the server response
       * append it to the result->writeConcernErrors array */
      bson_iter_document (&iter, &len, &data);
      bson_init_static (&write_concern_error, data, len);

      bson_uint32_to_string (
         result->n_writeConcernErrors, &key, str, sizeof str);

      bson_append_document (
         &result->writeConcernErrors, key, -1, &write_concern_error);

      result->n_writeConcernErrors++;
   }

   EXIT;
}


/*
 * If error is not set, set code from first document in array like
 * [{"code": 64, "errmsg": "duplicate"}, ...]. Format the error message
 * from all errors in array.
*/
static void
_set_error_from_response (bson_t *bson_array,
                          mongoc_error_domain_t domain,
                          const char *error_type,
                          bson_error_t *error /* OUT */)
{
   bson_iter_t array_iter;
   bson_iter_t doc_iter;
   bson_string_t *compound_err;
   const char *errmsg = NULL;
   int32_t code = 0;
   uint32_t n_keys, i;

   compound_err = bson_string_new (NULL);
   n_keys = bson_count_keys (bson_array);
   if (n_keys > 1) {
      bson_string_append_printf (
         compound_err, "Multiple %s errors: ", error_type);
   }

   if (!bson_empty0 (bson_array) && bson_iter_init (&array_iter, bson_array)) {
      /* get first code and all error messages */
      i = 0;

      while (bson_iter_next (&array_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&array_iter) &&
             bson_iter_recurse (&array_iter, &doc_iter)) {
            /* parse doc, which is like {"code": 64, "errmsg": "duplicate"} */
            while (bson_iter_next (&doc_iter)) {
               /* use the first error code we find */
               if (BSON_ITER_IS_KEY (&doc_iter, "code") && code == 0) {
                  code = bson_iter_int32 (&doc_iter);
               } else if (BSON_ITER_IS_KEY (&doc_iter, "errmsg")) {
                  errmsg = bson_iter_utf8 (&doc_iter, NULL);

                  /* build message like 'Multiple write errors: "foo", "bar"' */
                  if (n_keys > 1) {
                     bson_string_append_printf (compound_err, "\"%s\"", errmsg);
                     if (i < n_keys - 1) {
                        bson_string_append (compound_err, ", ");
                     }
                  } else {
                     /* single error message */
                     bson_string_append (compound_err, errmsg);
                  }
               }
            }

            i++;
         }
      }

      if (code && compound_err->len) {
         bson_set_error (
            error, domain, (uint32_t) code, "%s", compound_err->str);
      }
   }

   bson_string_free (compound_err, true);
}


bool
_mongoc_write_result_complete (
   mongoc_write_result_t *result,             /* IN */
   int32_t error_api_version,                 /* IN */
   const mongoc_write_concern_t *wc,          /* IN */
   mongoc_error_domain_t err_domain_override, /* IN */
   bson_t *bson,                              /* OUT */
   bson_error_t *error)                       /* OUT */
{
   mongoc_error_domain_t domain;

   ENTRY;

   BSON_ASSERT (result);

   if (error_api_version >= MONGOC_ERROR_API_VERSION_2) {
      domain = MONGOC_ERROR_SERVER;
   } else if (err_domain_override) {
      domain = err_domain_override;
   } else if (result->error.domain) {
      domain = (mongoc_error_domain_t) result->error.domain;
   } else {
      domain = MONGOC_ERROR_COLLECTION;
   }

   if (bson && mongoc_write_concern_is_acknowledged (wc)) {
      BSON_APPEND_INT32 (bson, "nInserted", result->nInserted);
      BSON_APPEND_INT32 (bson, "nMatched", result->nMatched);
      if (!result->omit_nModified) {
         BSON_APPEND_INT32 (bson, "nModified", result->nModified);
      }
      BSON_APPEND_INT32 (bson, "nRemoved", result->nRemoved);
      BSON_APPEND_INT32 (bson, "nUpserted", result->nUpserted);
      if (!bson_empty0 (&result->upserted)) {
         BSON_APPEND_ARRAY (bson, "upserted", &result->upserted);
      }
      BSON_APPEND_ARRAY (bson, "writeErrors", &result->writeErrors);
      if (result->n_writeConcernErrors) {
         BSON_APPEND_ARRAY (
            bson, "writeConcernErrors", &result->writeConcernErrors);
      }
   }

   /* set bson_error_t from first write error or write concern error */
   _set_error_from_response (
      &result->writeErrors, domain, "write", &result->error);

   if (!result->error.code) {
      _set_error_from_response (&result->writeConcernErrors,
                                MONGOC_ERROR_WRITE_CONCERN,
                                "write concern",
                                &result->error);
   }

   if (error) {
      memcpy (error, &result->error, sizeof *error);
   }

   RETURN (!result->failed && result->error.code == 0);
}
