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


#include "mongoc-client-private.h"
#include "mongoc-error.h"
#include "mongoc-flags.h"
#include "mongoc-rpc-private.h"
#include "mongoc-trace.h"
#include "mongoc-write-command-private.h"
#include "mongoc-write-concern-private.h"


/*
 * TODO:
 *
 *    - Remove error parameter to ops, favor result->error.
 *    - Try to receive GLE on the stack (legacy only?)
 */


#define MAX_INSERT_BATCH 1000
#define SUPPORTS_WRITE_COMMANDS(n) \
   (((n)->min_wire_version <= 2) && ((n)->max_wire_version >= 2))
#define WRITE_CONCERN_DOC(wc) \
   (wc && _mongoc_write_concern_needs_gle ((wc))) ? \
   (_mongoc_write_concern_get_bson((mongoc_write_concern_t*)(wc))) : \
   (&gEmptyWriteConcern)


typedef void (*mongoc_write_op_t) (mongoc_write_command_t       *command,
                                   mongoc_client_t              *client,
                                   uint32_t                      hint,
                                   const char                   *database,
                                   const char                   *collection,
                                   const mongoc_write_concern_t *write_concern,
                                   mongoc_write_result_t        *result,
                                   bson_error_t                 *error);


static bson_t gEmptyWriteConcern = BSON_INITIALIZER;

static int32_t
_mongoc_write_result_merge_arrays (mongoc_write_result_t *result,
                                   bson_t                *dest,
                                   bson_iter_t           *iter);
void
_mongoc_write_command_insert_append (mongoc_write_command_t *command,
                                     const bson_t * const   *documents,
                                     uint32_t                n_documents)
{
   const char *key;
   bson_iter_t iter;
   bson_oid_t oid;
   uint32_t i;
   bson_t tmp;
   char keydata [16];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (!n_documents || documents);

   for (i = 0; i < n_documents; i++) {
      BSON_ASSERT (documents [i]);
      BSON_ASSERT (documents [i]->len >= 5);

      key = NULL;
      bson_uint32_to_string (i, &key, keydata, sizeof keydata);
      BSON_ASSERT (key);

      /*
       * If the document does not contain an "_id" field, we need to generate
       * a new oid for "_id".
       */
      if (!bson_iter_init_find (&iter, documents [i], "_id")) {
         bson_init (&tmp);
         bson_oid_init (&oid, NULL);
         BSON_APPEND_OID (&tmp, "_id", &oid);
         bson_concat (&tmp, documents [i]);
         BSON_APPEND_DOCUMENT (command->u.insert.documents, key, &tmp);
         bson_destroy (&tmp);
      } else {
         BSON_APPEND_DOCUMENT (command->u.insert.documents, key,
                               documents [i]);
      }
   }

   if (command->u.insert.n_documents) {
      command->u.insert.n_merged++;
   }

   command->u.insert.n_documents += n_documents;

   EXIT;
}


void
_mongoc_write_command_init_insert
      (mongoc_write_command_t *command,              /* IN */
       const bson_t * const   *documents,            /* IN */
       uint32_t                n_documents,          /* IN */
       bool                    ordered,              /* IN */
       bool                    allow_bulk_op_insert) /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (!n_documents || documents);

   command->type = MONGOC_WRITE_COMMAND_INSERT;
   command->u.insert.documents = bson_new ();
   command->u.insert.n_documents = 0;
   command->u.insert.n_merged = 0;
   command->u.insert.ordered = ordered;
   command->u.insert.allow_bulk_op_insert = allow_bulk_op_insert;

   if (n_documents) {
      _mongoc_write_command_insert_append (command, documents, n_documents);
   }

   EXIT;
}


void
_mongoc_write_command_init_delete (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);

   command->type = MONGOC_WRITE_COMMAND_DELETE;
   command->u.delete.selector = bson_copy (selector);
   command->u.delete.multi = multi;
   command->u.delete.ordered = ordered;

   EXIT;
}


void
_mongoc_write_command_init_update (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   const bson_t           *update,   /* IN */
                                   bool                    upsert,   /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   command->type = MONGOC_WRITE_COMMAND_UPDATE;
   command->u.update.selector = bson_copy (selector);
   command->u.update.update = bson_copy (update);
   command->u.update.upsert = upsert;
   command->u.update.multi = multi;
   command->u.update.ordered = ordered;

   EXIT;
}


static void
_mongoc_write_command_delete_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_rpc_t rpc;
   bson_t *gle = NULL;
   char ns [MONGOC_NAMESPACE_MAX + 1];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 0;
   rpc.delete.response_to = 0;
   rpc.delete.opcode = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   rpc.delete.collection = ns;
   rpc.delete.flags = command->u.delete.multi ? 0 : MONGOC_DELETE_SINGLE_REMOVE;
   rpc.delete.selector = bson_get_data (command->u.delete.selector);

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }
   }

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
   }

   EXIT;
}


static void
_mongoc_write_command_insert_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_iovec_t *iov;
   const uint8_t *data;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   uint32_t len;
   bson_t *gle = NULL;
   size_t size = 0;
   bool has_more = false;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   bool r;
   int i;
   int max_docs = MAX_INSERT_BATCH;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);

   if (command->u.insert.ordered || !command->u.insert.allow_bulk_op_insert) {
      max_docs = 1;
   }

   r = bson_iter_init (&iter, command->u.insert.documents);
   if (!r) {
      BSON_ASSERT (false);
      EXIT;
   }

   if (!command->u.insert.n_documents || !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      "Cannot do an empty insert.");
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   iov = bson_malloc ((sizeof *iov) * command->u.insert.n_documents);

again:
   has_more = false;
   i = 0;
   size = (sizeof (mongoc_rpc_header_t) +
           4 +
           strlen (database) +
           1 +
           strlen (collection) +
           1);

   do {
      BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
      BSON_ASSERT (i < command->u.insert.n_documents);

      bson_iter_document (&iter, &len, &data);

      BSON_ASSERT (data);
      BSON_ASSERT (len >= 5);

      /*
       * Check that the server can receive this document.
       */
      if ((len > client->cluster.max_bson_size) ||
          (len > client->cluster.max_msg_size)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Document %u is too large for the cluster. "
                         "Document is %u bytes, max is %u.",
                         i, (unsigned)len, client->cluster.max_bson_size);
      }

      /*
       * Check that we will not overflow our max message size.
       */
      if ((i == max_docs) || (size > (client->cluster.max_msg_size - len))) {
         has_more = true;
         break;
      }

      iov [i].iov_base = (void *)data;
      iov [i].iov_len = len;

      size += len;
      i++;
   } while (bson_iter_next (&iter));

   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 0;
   rpc.insert.response_to = 0;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags =
      (command->u.insert.ordered ? 0 : MONGOC_INSERT_CONTINUE_ON_ERROR);
   rpc.insert.collection = ns;
   rpc.insert.documents = iov;
   rpc.insert.n_documents = i;

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      bson_iter_t citer;

      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }

      /*
       * Overwrite the "n" field since it will be zero. Otherwise, our
       * merge_legacy code will not know how many we tried in this batch.
       */
      if (bson_iter_init_find (&citer, gle, "n") &&
          BSON_ITER_HOLDS_INT32 (&citer) &&
          !bson_iter_int32 (&citer)) {
         bson_iter_overwrite_int32 (&citer, i);
      }
   }

cleanup:
   if (gle) {
      command->u.insert.current_n_documents = i;
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
      gle = NULL;
   }

   if (has_more) {
      GOTO (again);
   }

   bson_free (iov);

   EXIT;
}


static void
_mongoc_write_command_update_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bson_t *gle = NULL;
   size_t err_offset;
   char ns [MONGOC_NAMESPACE_MAX + 1];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   if (bson_iter_init (&iter, command->u.update.update) &&
       bson_iter_next (&iter) &&
       (bson_iter_key (&iter) [0] != '$') &&
       !bson_validate (command->u.update.update,
                       (BSON_VALIDATE_UTF8 |
                        BSON_VALIDATE_UTF8_ALLOW_NULL |
                        BSON_VALIDATE_DOLLAR_KEYS |
                        BSON_VALIDATE_DOT_KEYS),
                       &err_offset)) {
      result->failed = true;
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "update document is corrupt or contains "
                      "invalid keys including $ or .");
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   rpc.update.msg_len = 0;
   rpc.update.request_id = 0;
   rpc.update.response_to = 0;
   rpc.update.opcode = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   rpc.update.collection = ns;
   rpc.update.flags =
      ((command->u.update.upsert ? MONGOC_UPDATE_UPSERT : 0) |
       (command->u.update.multi ? MONGOC_UPDATE_MULTI_UPDATE : 0));
   rpc.update.selector = bson_get_data (command->u.update.selector);
   rpc.update.update = bson_get_data (command->u.update.update);

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }
   }

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
   }

   EXIT;
}


static void
_mongoc_write_command_delete (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t ar;
   bson_t child;
   bson_t reply;
   bool ret;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_delete_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   BSON_APPEND_UTF8 (&cmd, "delete", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.delete.ordered);
   bson_append_array_begin (&cmd, "deletes", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", command->u.delete.selector);
   BSON_APPEND_INT32 (&child, "limit", command->u.delete.multi ? 0 : 1);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (&cmd, &ar);

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   EXIT;
}


static void
_mongoc_write_command_insert (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   const uint8_t *data;
   bson_iter_t iter;
   const char *key;
   uint32_t len;
   bson_t tmp;
   bson_t ar;
   bson_t cmd;
   bson_t reply;
   char str [16];
   size_t overhead;
   bool has_more;
   bool ret;
   int i;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_insert_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   if (!command->u.insert.n_documents ||
       !bson_iter_init (&iter, command->u.insert.documents) ||
       !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      "Cannot do an empty insert.");
      result->failed = true;
      EXIT;
   }

   overhead = 1 + strlen ("documents") + 1;

again:
   bson_init (&cmd);
   has_more = false;
   i = 0;

   BSON_APPEND_UTF8 (&cmd, "insert", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.insert.ordered);

   if ((command->u.insert.documents->len < client->cluster.max_bson_size) &&
       (command->u.insert.documents->len < client->cluster.max_msg_size) &&
       (command->u.insert.n_documents <= MAX_INSERT_BATCH)) {
      BSON_APPEND_ARRAY (&cmd, "documents", command->u.insert.documents);
   } else {
      bson_append_array_begin (&cmd, "documents", 9, &ar);

      do {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            BSON_ASSERT (false);
         }

         bson_iter_document (&iter, &len, &data);

         if ((i == MAX_INSERT_BATCH) ||
             (len > (client->cluster.max_msg_size - cmd.len - overhead))) {
            has_more = true;
            break;
         }

         bson_uint32_to_string (i, &key, str, sizeof str);

         if (!bson_init_static (&tmp, data, len)) {
            BSON_ASSERT (false);
         }

         BSON_APPEND_DOCUMENT (&ar, key, &tmp);

         bson_destroy (&tmp);

         i++;
      } while (bson_iter_next (&iter));

      bson_append_array_end (&cmd, &ar);
   }

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   command->u.insert.current_n_documents = i;
   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&cmd);
   bson_destroy (&reply);

   if (has_more && (ret || !command->u.insert.ordered)) {
      GOTO (again);
   }

   EXIT;
}


static void
_mongoc_write_command_update (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bson_t ar;
   bson_t child;
   bool ret;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_update_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   BSON_APPEND_UTF8 (&cmd, "update", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.update.ordered);
   bson_append_array_begin (&cmd, "updates", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", command->u.update.selector);
   BSON_APPEND_DOCUMENT (&child, "u", command->u.update.update);
   BSON_APPEND_BOOL (&child, "multi", command->u.update.multi);
   BSON_APPEND_BOOL (&child, "upsert", command->u.update.upsert);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (&cmd, &ar);

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   EXIT;
}


static mongoc_write_op_t gWriteOps [2][3] = {
   { _mongoc_write_command_delete_legacy,
     _mongoc_write_command_insert_legacy,
     _mongoc_write_command_update_legacy },
   { _mongoc_write_command_delete,
     _mongoc_write_command_insert,
     _mongoc_write_command_update },
};


void
_mongoc_write_command_execute (mongoc_write_command_t       *command,       /* IN */
                               mongoc_client_t              *client,        /* IN */
                               uint32_t                      hint,          /* IN */
                               const char                   *database,      /* IN */
                               const char                   *collection,    /* IN */
                               const mongoc_write_concern_t *write_concern, /* IN */
                               mongoc_write_result_t        *result)        /* OUT */
{
   mongoc_cluster_node_t *node;
   int mode = 0;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (collection);
   BSON_ASSERT (result);

   if (!write_concern) {
      write_concern = client->write_concern;
   }

   if (!hint) {
      hint = _mongoc_client_preselect (client, MONGOC_OPCODE_INSERT,
                                       write_concern, NULL, &result->error);
      if (!hint) {
         result->failed = true;
         EXIT;
      }
   }

   command->hint = hint;

   node = &client->cluster.nodes [hint - 1];
   mode = SUPPORTS_WRITE_COMMANDS (node);

   gWriteOps [mode][command->type] (command, client, hint, database,
                                    collection, write_concern, result,
                                    &result->error);

   EXIT;
}


void
_mongoc_write_command_destroy (mongoc_write_command_t *command)
{
   ENTRY;

   if (command) {
      switch (command->type) {
      case MONGOC_WRITE_COMMAND_DELETE:
         bson_destroy (command->u.delete.selector);
         break;
      case MONGOC_WRITE_COMMAND_INSERT:
         bson_destroy (command->u.insert.documents);
         break;
      case MONGOC_WRITE_COMMAND_UPDATE:
         bson_destroy (command->u.update.selector);
         bson_destroy (command->u.update.update);
         break;
      default:
         BSON_ASSERT (false);
         break;
      }
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
   bson_init (&result->writeConcernError);
   bson_init (&result->writeErrors);

   EXIT;
}


void
_mongoc_write_result_destroy (mongoc_write_result_t *result)
{
   ENTRY;

   BSON_ASSERT (result);

   bson_destroy (&result->upserted);
   bson_destroy (&result->writeConcernError);
   bson_destroy (&result->writeErrors);

   EXIT;
}


static void
_mongoc_write_result_append_upsert (mongoc_write_result_t *result,
                                    int32_t                idx,
                                    const bson_value_t    *value)
{
   bson_t child;
   const char *keyptr = NULL;
   char key[12];
   int len;

   BSON_ASSERT (result);
   BSON_ASSERT (value);

   len = bson_uint32_to_string (result->upsert_append_count, &keyptr, key,
                                sizeof key);

   bson_append_document_begin (&result->upserted, keyptr, len, &child);
   BSON_APPEND_INT32 (&child, "index", idx);
   BSON_APPEND_VALUE (&child, "_id", value);
   bson_append_document_end (&result->upserted, &child);

   result->upsert_append_count++;
}


void
_mongoc_write_result_merge_legacy (mongoc_write_result_t  *result, /* IN */
                                   mongoc_write_command_t *command, /* IN */
                                   const bson_t           *reply)  /* IN */
{
   const bson_value_t *value;
   bson_t holder, write_errors, child;
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t citer;
   const char *err = NULL;
   int32_t code = 0;
   int32_t n = 0;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

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

   if (code && err) {
      bson_set_error (&result->error,
                      MONGOC_ERROR_COLLECTION,
                      code,
                      "%s", err);
      result->failed = true;

      bson_init(&holder);
      bson_append_array_begin(&holder, "0", 1, &write_errors);
      bson_append_document_begin(&write_errors, "0", 1, &child);
      bson_append_int32(&child, "index", 5, 0);
      bson_append_int32(&child, "code", 4, code);
      bson_append_utf8(&child, "errmsg", 6, err, -1);
      bson_append_document_end(&write_errors, &child);
      bson_append_array_end(&holder, &write_errors);
      bson_iter_init(&iter, &holder);
      bson_iter_next(&iter);

      _mongoc_write_result_merge_arrays (result, &result->writeErrors, &iter);

      bson_destroy(&holder);
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
          BSON_ITER_HOLDS_OID (&iter)) {
         result->nUpserted += 1;
         value = bson_iter_value (&iter);
         _mongoc_write_result_append_upsert (result, result->n_commands, value);
      } else if (bson_iter_init_find (&iter, reply, "upserted") &&
                 BSON_ITER_HOLDS_ARRAY (&iter)) {
         result->nUpserted += n;
         if (bson_iter_recurse (&iter, &ar)) {
            while (bson_iter_next (&ar)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                   bson_iter_recurse (&ar, &citer) &&
                   bson_iter_find (&citer, "_id")) {
                  value = bson_iter_value (&citer);
                  _mongoc_write_result_append_upsert (result,
                                                      result->n_commands,
                                                      value);
               }
            }
         }
      } else if ((n == 1) &&
                 bson_iter_init_find (&iter, reply, "updatedExisting") &&
                 BSON_ITER_HOLDS_BOOL (&iter) &&
                 !bson_iter_bool (&iter)) {
         /*
          * CDRIVER-372:
          *
          * Versions of MongoDB before 2.6 don't return the _id for an
          * upsert if _id is not an ObjectId.
          */
         result->nUpserted += 1;
         if (bson_iter_init_find (&iter, command->u.update.update, "_id") ||
             bson_iter_init_find (&iter, command->u.update.selector, "_id")) {
            value = bson_iter_value (&iter);
            _mongoc_write_result_append_upsert (result, result->n_commands,
                                                value);
         }
      } else {
         result->nMatched += n;
      }
      break;
   default:
      break;
   }

   result->omit_nModified = true;

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_DELETE:
   case MONGOC_WRITE_COMMAND_UPDATE:
      result->offset += 1;
      break;
   case MONGOC_WRITE_COMMAND_INSERT:
      result->offset += command->u.insert.current_n_documents;
      break;
   default:
      break;
   }

   result->n_commands++;

   if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
      result->n_commands += command->u.insert.n_merged;
   }

   EXIT;
}


static int32_t
_mongoc_write_result_merge_arrays (mongoc_write_result_t *result, /* IN */
                                   bson_t                *dest,   /* IN */
                                   bson_iter_t           *iter)   /* IN */
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
            len = bson_uint32_to_string (aridx++, &keyptr, key, sizeof key);
            bson_append_document_begin (dest, keyptr, len, &child);
            while (bson_iter_next (&citer)) {
               if (BSON_ITER_IS_KEY (&citer, "index")) {
                  idx = bson_iter_int32 (&citer) + result->offset;
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
_mongoc_write_result_merge (mongoc_write_result_t  *result,  /* IN */
                            mongoc_write_command_t *command, /* IN */
                            const bson_t           *reply)   /* IN */
{
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
       BSON_ITER_HOLDS_ARRAY (&iter) &&
       bson_iter_recurse (&iter, &citer) &&
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
      if (bson_iter_init_find (&iter, reply, "upserted")) {
         if (BSON_ITER_HOLDS_ARRAY (&iter)) {
            if (bson_iter_recurse (&iter, &ar)) {
               while (bson_iter_next (&ar)) {
                  if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                      bson_iter_recurse (&ar, &citer) &&
                      bson_iter_find (&citer, "_id")) {
                     value = bson_iter_value (&citer);
                     _mongoc_write_result_append_upsert (result,
                                                         result->n_commands,
                                                         value);
                     n_upserted++;
                  }
               }
            }
         } else {
            value = bson_iter_value (&iter);
            _mongoc_write_result_append_upsert (result, result->n_commands, value);
            n_upserted = 1;
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
      _mongoc_write_result_merge_arrays (result, &result->writeErrors, &iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeConcernError") &&
       BSON_ITER_HOLDS_DOCUMENT (&iter)) {

      uint32_t len;
      const uint8_t *data;
      bson_t write_concern_error;

      bson_iter_document (&iter, &len, &data);
      bson_init_static (&write_concern_error, data, len);
      bson_concat (&result->writeConcernError, &write_concern_error);
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_DELETE:
   case MONGOC_WRITE_COMMAND_UPDATE:
      result->offset += affected;
      break;
   case MONGOC_WRITE_COMMAND_INSERT:
      result->offset += command->u.insert.current_n_documents;
      break;
   default:
      break;
   }

   result->n_commands++;

   if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
      result->n_commands += command->u.insert.n_merged;
   }

   EXIT;
}


bool
_mongoc_write_result_complete (mongoc_write_result_t *result,
                               bson_t                *bson,
                               bson_error_t          *error)
{
   bson_iter_t iter;
   bson_iter_t citer;
   const char *err = NULL;
   uint32_t code = 0;
   bool ret;

   ENTRY;

   BSON_ASSERT (result);

   ret = (!result->failed &&
          bson_empty0 (&result->writeConcernError) &&
          bson_empty0 (&result->writeErrors));

   if (bson) {
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
      if (!bson_empty0 (&result->writeConcernError)) {
         BSON_APPEND_DOCUMENT (bson, "writeConcernError",
                            &result->writeConcernError);
      }
   }

   if (error) {
      memcpy (error, &result->error, sizeof *error);
   }

   if (!ret &&
       !bson_empty0 (&result->writeErrors) &&
       bson_iter_init (&iter, &result->writeErrors) &&
       bson_iter_next (&iter) &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &citer)) {
      while (bson_iter_next (&citer)) {
         if (BSON_ITER_IS_KEY (&citer, "errmsg")) {
            err = bson_iter_utf8 (&citer, NULL);
         } else if (BSON_ITER_IS_KEY (&citer, "code")) {
            code = bson_iter_int32 (&citer);
         }
      }
      if (err && code) {
         bson_set_error (error, MONGOC_ERROR_COMMAND, code, "%s", err);
      }
   }

   RETURN (ret);
}
