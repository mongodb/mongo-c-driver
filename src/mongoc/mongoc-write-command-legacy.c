/*
 * Copyright 2014-present MongoDB, Inc.
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

#include "mongoc-write-command-legacy-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-util-private.h"

static bool
_is_duplicate_key_error (int32_t code)
{
   return code == 11000 || code == 16460 || /* see SERVER-11493 */
          code == 11001 || /* duplicate key for updates before 2.6 */
          code == 12582;   /* mongos before 2.6 */
}


static bool
get_upserted_id (const bson_t *update, bson_value_t *upserted_id)
{
   bson_iter_t iter;
   bson_iter_t id_iter;

   /* Versions of MongoDB before 2.6 don't return the _id for an upsert if _id
    * is not an ObjectId, so find it in the update document's query "q" or
    * update "u". It must be in one or both: if it were in neither the _id
    * would be server-generated, therefore an ObjectId, therefore returned and
    * we wouldn't call this function. If _id is in both the update document
    * *and* the query spec the update document _id takes precedence.
    */

   bson_iter_init (&iter, update);

   if (bson_iter_find_descendant (&iter, "u._id", &id_iter)) {
      bson_value_copy (bson_iter_value (&id_iter), upserted_id);
      return true;
   } else {
      bson_iter_init (&iter, update);

      if (bson_iter_find_descendant (&iter, "q._id", &id_iter)) {
         bson_value_copy (bson_iter_value (&id_iter), upserted_id);
         return true;
      }
   }

   /* server bug? */
   return false;
}


static void
append_upserted (bson_t *doc, const bson_value_t *upserted_id)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;

   /* append upserted: [{index: 0, _id: upserted_id}]*/
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_value (&child, "_id", 3, upserted_id);
   bson_append_document_end (&array, &child);

   bson_append_array (doc, "upserted", 8, &array);

   bson_destroy (&array);
}

static void
append_write_concern_err (bson_t *doc, const char *errmsg, size_t errmsg_len)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;
   bson_t errinfo;

   BSON_ASSERT (errmsg);

   /* writeConcernErrors: [{code: 64,
    *                       errmsg: errmsg,
    *                       errInfo: {wtimeout: true}}] */
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "code", 4, 64);
   bson_append_utf8 (&child, "errmsg", 6, errmsg, (int) errmsg_len);
   bson_append_document_begin (&child, "errInfo", 7, &errinfo);
   bson_append_bool (&errinfo, "wtimeout", 8, true);
   bson_append_document_end (&child, &errinfo);
   bson_append_document_end (&array, &child);
   bson_append_array (doc, "writeConcernErrors", 18, &array);

   bson_destroy (&array);
}


static void
_append_write_concern_err_legacy (mongoc_write_result_t *result,
                                  const char *err,
                                  int32_t code)
{
   char str[16];
   const char *key;
   size_t keylen;
   bson_t write_concern_error;

   /* don't set result->failed; record the write concern err and continue */
   keylen = bson_uint32_to_string (
      result->n_writeConcernErrors, &key, str, sizeof str);

   BSON_ASSERT (keylen < INT_MAX);

   bson_append_document_begin (
      &result->writeConcernErrors, key, (int) keylen, &write_concern_error);

   bson_append_int32 (&write_concern_error, "code", 4, code);
   bson_append_utf8 (&write_concern_error, "errmsg", 6, err, -1);
   bson_append_document_end (&result->writeConcernErrors, &write_concern_error);
   result->n_writeConcernErrors++;
}

static void
append_write_err (bson_t *doc,
                  uint32_t code,
                  const char *errmsg,
                  size_t errmsg_len,
                  const bson_t *errinfo)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;

   BSON_ASSERT (errmsg);

   /* writeErrors: [{index: 0, code: code, errmsg: errmsg, errInfo: {...}}] */
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_int32 (&child, "code", 4, (int32_t) code);
   bson_append_utf8 (&child, "errmsg", 6, errmsg, (int) errmsg_len);
   if (errinfo) {
      bson_append_document (&child, "errInfo", 7, errinfo);
   }

   bson_append_document_end (&array, &child);
   bson_append_array (doc, "writeErrors", 11, &array);

   bson_destroy (&array);
}


static void
_append_write_err_legacy (mongoc_write_result_t *result,
                          const char *err,
                          mongoc_error_domain_t domain,
                          int32_t code,
                          uint32_t offset)
{
   bson_t holder, write_errors, child;
   bson_iter_t iter;

   BSON_ASSERT (code > 0);

   if (!result->error.domain) {
      bson_set_error (&result->error, domain, (uint32_t) code, "%s", err);
   }

   /* stop processing, if result->ordered */
   result->failed = true;

   bson_init (&holder);
   bson_append_array_begin (&holder, "0", 1, &write_errors);
   bson_append_document_begin (&write_errors, "0", 1, &child);

   /* set error's "index" to 0; fixed up in _mongoc_write_result_merge_arrays */
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_int32 (&child, "code", 4, code);
   bson_append_utf8 (&child, "errmsg", 6, err, -1);
   bson_append_document_end (&write_errors, &child);
   bson_append_array_end (&holder, &write_errors);
   bson_iter_init (&iter, &holder);
   bson_iter_next (&iter);

   _mongoc_write_result_merge_arrays (
      offset, result, &result->writeErrors, &iter);

   bson_destroy (&holder);
}


void
_mongoc_write_result_merge_legacy (mongoc_write_result_t *result,   /* IN */
                                   mongoc_write_command_t *command, /* IN */
                                   const bson_t *reply,             /* IN */
                                   int32_t error_api_version,
                                   mongoc_error_code_t default_code,
                                   uint32_t offset)
{
   const bson_value_t *value;
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t citer;
   const char *err = NULL;
   int32_t code = 0;
   int32_t n = 0;
   int32_t upsert_idx = 0;
   mongoc_error_domain_t domain;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

   domain = error_api_version >= MONGOC_ERROR_API_VERSION_2
               ? MONGOC_ERROR_SERVER
               : MONGOC_ERROR_COLLECTION;

   if (bson_iter_init_find (&iter, reply, "n") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      n = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, reply, "err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      err = bson_iter_utf8 (&iter, NULL);
   }

   if (bson_iter_init_find (&iter, reply, "code") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      code = bson_iter_int32 (&iter);
   }

   if (_is_duplicate_key_error (code)) {
      code = MONGOC_ERROR_DUPLICATE_KEY;
   }

   if (code || err) {
      if (!err) {
         err = "unknown error";
      }

      if (bson_iter_init_find (&iter, reply, "wtimeout") &&
          bson_iter_as_bool (&iter)) {
         if (!code) {
            code = (int32_t) MONGOC_ERROR_WRITE_CONCERN_ERROR;
         }

         _append_write_concern_err_legacy (result, err, code);
      } else {
         if (!code) {
            code = (int32_t) default_code;
         }

         _append_write_err_legacy (result, err, domain, code, offset);
      }
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_INSERT:
      if (n) {
         result->nInserted += n;
      }
      break;
   case MONGOC_WRITE_COMMAND_DELETE:
      result->nRemoved += n;
      break;
   case MONGOC_WRITE_COMMAND_UPDATE:
      if (bson_iter_init_find (&iter, reply, "upserted") &&
          !BSON_ITER_HOLDS_ARRAY (&iter)) {
         result->nUpserted += n;
         value = bson_iter_value (&iter);
         _mongoc_write_result_append_upsert (result, offset, value);
      } else if (bson_iter_init_find (&iter, reply, "upserted") &&
                 BSON_ITER_HOLDS_ARRAY (&iter)) {
         result->nUpserted += n;
         if (bson_iter_recurse (&iter, &ar)) {
            while (bson_iter_next (&ar)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                   bson_iter_recurse (&ar, &citer) &&
                   bson_iter_find (&citer, "_id")) {
                  value = bson_iter_value (&citer);
                  _mongoc_write_result_append_upsert (
                     result, offset + upsert_idx, value);
                  upsert_idx++;
               }
            }
         }
      } else if ((n == 1) &&
                 bson_iter_init_find (&iter, reply, "updatedExisting") &&
                 BSON_ITER_HOLDS_BOOL (&iter) && !bson_iter_bool (&iter)) {
         result->nUpserted += n;
      } else {
         result->nMatched += n;
      }
      break;
   default:
      break;
   }

   result->omit_nModified = true;

   EXIT;
}


static void
_mongoc_monitor_legacy_write (mongoc_client_t *client,
                              mongoc_write_command_t *command,
                              const char *db,
                              const char *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_server_stream_t *stream,
                              int64_t request_id)
{
   bson_t doc;
   mongoc_apm_command_started_t event;

   ENTRY;

   if (!client->apm_callbacks.started) {
      EXIT;
   }

   bson_init (&doc);
   _mongoc_write_command_init (&doc, command, collection, write_concern);
   _append_array_from_command (command, &doc);

   mongoc_apm_command_started_init (
      &event,
      &doc,
      db,
      _mongoc_command_type_to_name (command->type),
      request_id,
      command->operation_id,
      &stream->sd->host,
      stream->sd->id,
      client->apm_context);

   client->apm_callbacks.started (&event);

   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);
}


/* fire command-succeeded event as if we'd used a modern write command.
 * note, cluster.request_id was incremented once for the write, again
 * for the getLastError, so cluster.request_id is no longer valid; used the
 * passed-in request_id instead.
 */
static void
_mongoc_monitor_legacy_write_succeeded (mongoc_client_t *client,
                                        int64_t duration,
                                        mongoc_write_command_t *command,
                                        const bson_t *gle,
                                        mongoc_server_stream_t *stream,
                                        int64_t request_id)
{
   bson_iter_t iter;
   bson_t doc;
   int64_t ok = 1;
   int64_t n = 0;
   uint32_t code = 8;
   bool wtimeout = false;

   /* server error message */
   const char *errmsg = NULL;
   size_t errmsg_len = 0;

   /* server errInfo subdocument */
   bool has_errinfo = false;
   uint32_t len;
   const uint8_t *data;
   bson_t errinfo;

   /* server upsertedId value */
   bool has_upserted_id = false;
   bson_value_t upserted_id;

   /* server updatedExisting value */
   bool has_updated_existing = false;
   bool updated_existing = false;

   mongoc_apm_command_succeeded_t event;

   ENTRY;

   if (!client->apm_callbacks.succeeded) {
      EXIT;
   }

   /* first extract interesting fields from getlasterror response */
   if (gle) {
      bson_iter_init (&iter, gle);
      while (bson_iter_next (&iter)) {
         if (!strcmp (bson_iter_key (&iter), "ok")) {
            ok = bson_iter_as_int64 (&iter);
         } else if (!strcmp (bson_iter_key (&iter), "n")) {
            n = bson_iter_as_int64 (&iter);
         } else if (!strcmp (bson_iter_key (&iter), "code")) {
            code = (uint32_t) bson_iter_as_int64 (&iter);
            if (code == 0) {
               /* server sent non-numeric error code? */
               code = 8;
            }
         } else if (!strcmp (bson_iter_key (&iter), "upserted")) {
            has_upserted_id = true;
            bson_value_copy (bson_iter_value (&iter), &upserted_id);
         } else if (!strcmp (bson_iter_key (&iter), "updatedExisting")) {
            has_updated_existing = true;
            updated_existing = bson_iter_as_bool (&iter);
         } else if ((!strcmp (bson_iter_key (&iter), "err") ||
                     !strcmp (bson_iter_key (&iter), "errmsg")) &&
                    BSON_ITER_HOLDS_UTF8 (&iter)) {
            errmsg = bson_iter_utf8_unsafe (&iter, &errmsg_len);
         } else if (!strcmp (bson_iter_key (&iter), "errInfo") &&
                    BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            bson_iter_document (&iter, &len, &data);
            bson_init_static (&errinfo, data, len);
            has_errinfo = true;
         } else if (!strcmp (bson_iter_key (&iter), "wtimeout")) {
            wtimeout = true;
         }
      }
   }

   /* based on PyMongo's _convert_write_result() */
   bson_init (&doc);
   bson_append_int32 (&doc, "ok", 2, (int32_t) ok);

   if (errmsg && !wtimeout) {
      /* Failure, but pass to the success callback. Command Monitoring Spec:
       * "Commands that executed on the server and return a status of {ok: 1}
       * are considered successful commands and fire CommandSucceededEvent.
       * Commands that have write errors are included since the actual command
       * did succeed, only writes failed." */
      append_write_err (
         &doc, code, errmsg, errmsg_len, has_errinfo ? &errinfo : NULL);
   } else {
      /* Success, perhaps with a writeConcernError. */
      if (errmsg) {
         append_write_concern_err (&doc, errmsg, errmsg_len);
      }

      if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
         /* GLE result for insert is always 0 in most MongoDB versions. */
         n = command->n_documents;
      } else if (command->type == MONGOC_WRITE_COMMAND_UPDATE) {
         if (has_upserted_id) {
            append_upserted (&doc, &upserted_id);
         } else if (has_updated_existing && !updated_existing && n == 1) {
            bson_t tmp;
            int32_t bson_len = 0;

            memcpy (&bson_len, command->payload.data, 4);
            bson_len = BSON_UINT32_FROM_LE (bson_len);
            bson_init_static (&tmp, command->payload.data, bson_len);
            has_upserted_id = get_upserted_id (&tmp, &upserted_id);

            if (has_upserted_id) {
               append_upserted (&doc, &upserted_id);
            }
         }
      }
   }

   bson_append_int32 (&doc, "n", 1, (int32_t) n);

   mongoc_apm_command_succeeded_init (
      &event,
      duration,
      &doc,
      _mongoc_command_type_to_name (command->type),
      request_id,
      command->operation_id,
      &stream->sd->host,
      stream->sd->id,
      client->apm_context);

   client->apm_callbacks.succeeded (&event);

   mongoc_apm_command_succeeded_cleanup (&event);
   bson_destroy (&doc);

   if (has_upserted_id) {
      bson_value_destroy (&upserted_id);
   }

   EXIT;
}


void
_mongoc_write_command_delete_legacy (
   mongoc_write_command_t *command,
   mongoc_client_t *client,
   mongoc_server_stream_t *server_stream,
   const char *database,
   const char *collection,
   const mongoc_write_concern_t *write_concern,
   uint32_t offset,
   mongoc_write_result_t *result,
   bson_error_t *error)
{
   int64_t started;
   int32_t max_bson_obj_size;
   const uint8_t *data;
   mongoc_rpc_t rpc;
   uint32_t request_id;
   bson_iter_t q_iter;
   uint32_t len;
   int64_t limit = 0;
   bson_t *gle = NULL;
   char ns[MONGOC_NAMESPACE_MAX + 1];
   bool r;
   bson_reader_t *reader;
   const bson_t *bson;
   bool eof;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   started = bson_get_monotonic_time ();

   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);

   if (!command->n_documents) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_DELETE_FAILED,
                      "Cannot do an empty delete.");
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   reader =
      bson_reader_new_from_data (command->payload.data, command->payload.len);
   while ((bson = bson_reader_read (reader, &eof))) {
      /* the document is like { "q": { <selector> }, limit: <0 or 1> } */
      r = (bson_iter_init (&q_iter, bson) && bson_iter_find (&q_iter, "q") &&
           BSON_ITER_HOLDS_DOCUMENT (&q_iter));

      BSON_ASSERT (r);
      bson_iter_document (&q_iter, &len, &data);
      BSON_ASSERT (data);
      BSON_ASSERT (len >= 5);
      if (len > max_bson_obj_size) {
         _mongoc_write_command_too_large_error (
            error, 0, len, max_bson_obj_size, NULL);
         result->failed = true;
         EXIT;
      }

      request_id = ++client->cluster.request_id;

      rpc.header.msg_len = 0;
      rpc.header.request_id = request_id;
      rpc.header.response_to = 0;
      rpc.header.opcode = MONGOC_OPCODE_DELETE;
      rpc.delete_.zero = 0;
      rpc.delete_.collection = ns;

      if (bson_iter_find (&q_iter, "limit") &&
          (BSON_ITER_HOLDS_INT (&q_iter))) {
         limit = bson_iter_as_int64 (&q_iter);
      }

      rpc.delete_.flags =
         limit ? MONGOC_DELETE_SINGLE_REMOVE : MONGOC_DELETE_NONE;
      rpc.delete_.selector = data;

      _mongoc_monitor_legacy_write (client,
                                    command,
                                    database,
                                    collection,
                                    write_concern,
                                    server_stream,
                                    request_id);

      if (!mongoc_cluster_legacy_rpc_sendv_to_server (
             &client->cluster, &rpc, server_stream, write_concern, error)) {
         result->failed = true;
         EXIT;
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            EXIT;
         }

         _mongoc_write_result_merge_legacy (
            result,
            command,
            gle,
            client->error_api_version,
            MONGOC_ERROR_COLLECTION_DELETE_FAILED,
            offset);

         offset++;
      }

      _mongoc_monitor_legacy_write_succeeded (client,
                                              bson_get_monotonic_time () -
                                                 started,
                                              command,
                                              gle,
                                              server_stream,
                                              request_id);

      if (gle) {
         bson_destroy (gle);
         gle = NULL;
      }

      started = bson_get_monotonic_time ();
   }
   bson_reader_destroy (reader);

   EXIT;
}


void
_mongoc_write_command_insert_legacy (
   mongoc_write_command_t *command,
   mongoc_client_t *client,
   mongoc_server_stream_t *server_stream,
   const char *database,
   const char *collection,
   const mongoc_write_concern_t *write_concern,
   uint32_t offset,
   mongoc_write_result_t *result,
   bson_error_t *error)
{
   int64_t started;
   uint32_t current_offset;
   mongoc_iovec_t *iov;
   mongoc_rpc_t rpc;
   bson_t *gle = NULL;
   uint32_t size = 0;
   bool has_more;
   char ns[MONGOC_NAMESPACE_MAX + 1];
   uint32_t n_docs_in_batch;
   uint32_t request_id = 0;
   uint32_t idx = 0;
   int32_t max_msg_size;
   int32_t max_bson_obj_size;
   bool singly;
   bson_reader_t *reader;
   const bson_t *bson;
   bool eof;
   int data_offset = 0;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);

   started = bson_get_monotonic_time ();
   current_offset = offset;

   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);
   max_msg_size = mongoc_server_stream_max_msg_size (server_stream);

   singly = !command->u.insert.allow_bulk_op_insert;

   if (!command->n_documents) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      "Cannot do an empty insert.");
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   iov = (mongoc_iovec_t *) bson_malloc ((sizeof *iov) * command->n_documents);

again:
   has_more = false;
   n_docs_in_batch = 0;
   size = (uint32_t) (sizeof (mongoc_rpc_header_t) + 4 + strlen (database) + 1 +
                      strlen (collection) + 1);

   reader = bson_reader_new_from_data (command->payload.data + data_offset,
                                       command->payload.len - data_offset);
   while ((bson = bson_reader_read (reader, &eof))) {
      BSON_ASSERT (n_docs_in_batch <= idx);
      BSON_ASSERT (idx <= command->n_documents);

      if (bson->len > max_bson_obj_size) {
         /* document is too large */
         bson_t write_err_doc = BSON_INITIALIZER;

         _mongoc_write_command_too_large_error (
            error, idx, bson->len, max_bson_obj_size, &write_err_doc);

         _mongoc_write_result_merge_legacy (
            result,
            command,
            &write_err_doc,
            client->error_api_version,
            MONGOC_ERROR_COLLECTION_INSERT_FAILED,
            offset + idx);

         bson_destroy (&write_err_doc);
         data_offset += bson->len;

         if (command->flags.ordered) {
            /* send the batch so far (if any) and return the error */
            break;
         }
      } else if ((n_docs_in_batch == 1 && singly) ||
                 size > (max_msg_size - bson->len)) {
         /* batch is full, send it and then start the next batch */
         has_more = true;
         break;
      } else {
         /* add document to batch and continue building the batch */
         iov[n_docs_in_batch].iov_base = (void *) bson_get_data (bson);
         iov[n_docs_in_batch].iov_len = bson->len;
         size += bson->len;
         n_docs_in_batch++;
         data_offset += bson->len;
      }

      idx++;
   }
   bson_reader_destroy (reader);

   if (n_docs_in_batch) {
      request_id = ++client->cluster.request_id;

      rpc.header.msg_len = 0;
      rpc.header.request_id = request_id;
      rpc.header.response_to = 0;
      rpc.header.opcode = MONGOC_OPCODE_INSERT;
      rpc.insert.flags =
         ((command->flags.ordered) ? MONGOC_INSERT_NONE
                                   : MONGOC_INSERT_CONTINUE_ON_ERROR);
      rpc.insert.collection = ns;
      rpc.insert.documents = iov;
      rpc.insert.n_documents = n_docs_in_batch;

      _mongoc_monitor_legacy_write (client,
                                    command,
                                    database,
                                    collection,
                                    write_concern,
                                    server_stream,
                                    request_id);

      if (!mongoc_cluster_legacy_rpc_sendv_to_server (
             &client->cluster, &rpc, server_stream, write_concern, error)) {
         result->failed = true;
         GOTO (cleanup);
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         bool err = false;
         bson_iter_t citer;

         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            GOTO (cleanup);
         }

         err = (bson_iter_init_find (&citer, gle, "err") &&
                bson_iter_as_bool (&citer));

         /*
          * Overwrite the "n" field since it will be zero. Otherwise, our
          * merge_legacy code will not know how many we tried in this batch.
          */
         if (!err && bson_iter_init_find (&citer, gle, "n") &&
             BSON_ITER_HOLDS_INT32 (&citer) && !bson_iter_int32 (&citer)) {
            bson_iter_overwrite_int32 (&citer, n_docs_in_batch);
         }
      }

      _mongoc_monitor_legacy_write_succeeded (client,
                                              bson_get_monotonic_time () -
                                                 started,
                                              command,
                                              gle,
                                              server_stream,
                                              request_id);

      started = bson_get_monotonic_time ();
   }

cleanup:

   if (gle) {
      _mongoc_write_result_merge_legacy (result,
                                         command,
                                         gle,
                                         client->error_api_version,
                                         MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                                         current_offset);

      current_offset = offset + idx;
      bson_destroy (gle);
      gle = NULL;
   }

   if (has_more) {
      GOTO (again);
   }

   bson_free (iov);

   EXIT;
}


void
_mongoc_write_command_update_legacy (
   mongoc_write_command_t *command,
   mongoc_client_t *client,
   mongoc_server_stream_t *server_stream,
   const char *database,
   const char *collection,
   const mongoc_write_concern_t *write_concern,
   uint32_t offset,
   mongoc_write_result_t *result,
   bson_error_t *error)
{
   int64_t started;
   int32_t max_bson_obj_size;
   mongoc_rpc_t rpc;
   uint32_t request_id = 0;
   bson_iter_t subiter, subsubiter;
   bson_t doc;
   bool has_update, has_selector, is_upsert;
   bson_t update, selector;
   bson_t *gle = NULL;
   const uint8_t *data = NULL;
   uint32_t len = 0;
   size_t err_offset;
   bool val = false;
   char ns[MONGOC_NAMESPACE_MAX + 1];
   int32_t affected = 0;
   int vflags = (BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL |
                 BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS);
   bson_reader_t *reader;
   const bson_t *bson;
   bool eof;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   started = bson_get_monotonic_time ();

   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);

   reader =
      bson_reader_new_from_data (command->payload.data, command->payload.len);
   while ((bson = bson_reader_read (reader, &eof))) {
      if (bson_iter_init (&subiter, bson) && bson_iter_find (&subiter, "u") &&
          BSON_ITER_HOLDS_DOCUMENT (&subiter)) {
         bson_iter_document (&subiter, &len, &data);
         bson_init_static (&doc, data, len);

         if (bson_iter_init (&subsubiter, &doc) &&
             bson_iter_next (&subsubiter) &&
             (bson_iter_key (&subsubiter)[0] != '$') &&
             !bson_validate (
                &doc, (bson_validate_flags_t) vflags, &err_offset)) {
            result->failed = true;
            bson_set_error (error,
                            MONGOC_ERROR_BSON,
                            MONGOC_ERROR_BSON_INVALID,
                            "update document is corrupt or contains "
                            "invalid keys including $ or .");
            EXIT;
         }
      } else {
         result->failed = true;
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "updates is malformed.");
         EXIT;
      }
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   reader =
      bson_reader_new_from_data (command->payload.data, command->payload.len);
   while ((bson = bson_reader_read (reader, &eof))) {
      request_id = ++client->cluster.request_id;

      rpc.header.msg_len = 0;
      rpc.header.request_id = request_id;
      rpc.header.response_to = 0;
      rpc.header.opcode = MONGOC_OPCODE_UPDATE;
      rpc.update.zero = 0;
      rpc.update.collection = ns;
      rpc.update.flags = MONGOC_UPDATE_NONE;

      has_update = false;
      has_selector = false;
      is_upsert = false;

      bson_iter_init (&subiter, bson);
      while (bson_iter_next (&subiter)) {
         if (strcmp (bson_iter_key (&subiter), "u") == 0) {
            bson_iter_document (&subiter, &len, &data);
            if (len > max_bson_obj_size) {
               _mongoc_write_command_too_large_error (
                  error, 0, len, max_bson_obj_size, NULL);
               result->failed = true;
               EXIT;
            }

            rpc.update.update = data;
            bson_init_static (&update, data, len);
            has_update = true;
         } else if (strcmp (bson_iter_key (&subiter), "q") == 0) {
            bson_iter_document (&subiter, &len, &data);
            if (len > max_bson_obj_size) {
               _mongoc_write_command_too_large_error (
                  error, 0, len, max_bson_obj_size, NULL);
               result->failed = true;
               EXIT;
            }

            rpc.update.selector = data;
            bson_init_static (&selector, data, len);
            has_selector = true;
         } else if (strcmp (bson_iter_key (&subiter), "multi") == 0) {
            val = bson_iter_bool (&subiter);
            if (val) {
               rpc.update.flags = (mongoc_update_flags_t) (
                  rpc.update.flags | MONGOC_UPDATE_MULTI_UPDATE);
            }
         } else if (strcmp (bson_iter_key (&subiter), "upsert") == 0) {
            val = bson_iter_bool (&subiter);
            if (val) {
               rpc.update.flags = (mongoc_update_flags_t) (
                  rpc.update.flags | MONGOC_UPDATE_UPSERT);
            }
            is_upsert = true;
         }
      }

      _mongoc_monitor_legacy_write (client,
                                    command,
                                    database,
                                    collection,
                                    write_concern,
                                    server_stream,
                                    request_id);

      if (!mongoc_cluster_legacy_rpc_sendv_to_server (
             &client->cluster, &rpc, server_stream, write_concern, error)) {
         result->failed = true;
         EXIT;
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            EXIT;
         }

         if (bson_iter_init_find (&subiter, gle, "n") &&
             BSON_ITER_HOLDS_INT32 (&subiter)) {
            affected = bson_iter_int32 (&subiter);
         }

         /*
          * CDRIVER-372:
          *
          * Versions of MongoDB before 2.6 don't return the _id for an
          * upsert if _id is not an ObjectId.
          */
         if (is_upsert && affected &&
             !bson_iter_init_find (&subiter, gle, "upserted") &&
             bson_iter_init_find (&subiter, gle, "updatedExisting") &&
             BSON_ITER_HOLDS_BOOL (&subiter) && !bson_iter_bool (&subiter)) {
            if (has_update && bson_iter_init_find (&subiter, &update, "_id")) {
               _ignore_value (bson_append_iter (gle, "upserted", 8, &subiter));
            } else if (has_selector &&
                       bson_iter_init_find (&subiter, &selector, "_id")) {
               _ignore_value (bson_append_iter (gle, "upserted", 8, &subiter));
            }
         }

         _mongoc_write_result_merge_legacy (
            result,
            command,
            gle,
            client->error_api_version,
            MONGOC_ERROR_COLLECTION_UPDATE_FAILED,
            offset);

         offset++;
      }

      _mongoc_monitor_legacy_write_succeeded (client,
                                              bson_get_monotonic_time () -
                                                 started,
                                              command,
                                              gle,
                                              server_stream,
                                              request_id);

      if (gle) {
         bson_destroy (gle);
         gle = NULL;
      }

      started = bson_get_monotonic_time ();
   }
}
