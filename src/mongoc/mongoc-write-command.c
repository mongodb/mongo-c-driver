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


#define SUPPORTS_WRITE_COMMANDS(n) \
   (((n)->min_wire_version <= 2) && ((n)->max_wire_version >= 2))
#define WRITE_CONCERN_DOC(wc) \
   (wc) ? \
   (_mongoc_write_concern_freeze((mongoc_write_concern_t*)(wc))) : \
   (&gEmptyWriteConcern)


typedef bool (*mongoc_write_op_t) (mongoc_write_command_t       *command,
                                   mongoc_client_t              *client,
                                   uint32_t                      hint,
                                   const char                   *database,
                                   const char                   *collection,
                                   const mongoc_write_concern_t *write_concern,
                                   mongoc_write_result_t        *result,
                                   bson_error_t                 *error);


static bson_t gEmptyWriteConcern = BSON_INITIALIZER;


void
_mongoc_write_command_init_insert (mongoc_write_command_t *command,     /* IN */
                                   const bson_t * const   *documents,   /* IN */
                                   uint32_t                n_documents, /* IN */
                                   bool                    ordered)     /* IN */
{
   const char *key;
   uint32_t i;
   char keydata [12];

   BSON_ASSERT (command);

   command->type = MONGOC_WRITE_COMMAND_INSERT;
   command->u.insert.documents = bson_new ();
   command->u.insert.ordered = ordered;

   for (i = 0; i < n_documents; i++) {
      bson_uint32_to_string (i, &key, keydata, sizeof keydata);
      BSON_APPEND_DOCUMENT (command->u.insert.documents, key, documents [i]);
   }
}


void
_mongoc_write_command_init_delete (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   BSON_ASSERT (command);
   BSON_ASSERT (selector);

   command->type = MONGOC_WRITE_COMMAND_DELETE;
   command->u.delete.selector = bson_copy (selector);
   command->u.delete.multi = multi;
   command->u.delete.ordered = ordered;
}


void
_mongoc_write_command_init_update (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   const bson_t           *update,   /* IN */
                                   bool                    upsert,   /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   BSON_ASSERT (command);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   command->type = MONGOC_WRITE_COMMAND_UPDATE;
   command->u.update.selector = bson_copy (selector);
   command->u.update.update = bson_copy (update);
   command->u.update.upsert = upsert;
   command->u.update.multi = multi;
   command->u.update.ordered = ordered;
}


static bool
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
   bool ret = false;

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
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_has_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         GOTO (cleanup);
      }
   }

   ret = true;

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, gle);
      bson_destroy (gle);
   }

   RETURN (ret);
}


static bool
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
   bool ret = false;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   int i = 0;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   iov = bson_malloc ((sizeof *iov) * command->u.insert.n_documents);

   if (!bson_iter_init (&iter, command->u.insert.documents)) {
      BSON_ASSERT (false);
      RETURN (false);
   }

   while (bson_iter_next (&iter)) {
      BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
      BSON_ASSERT (i < command->u.insert.n_documents);
      bson_iter_document (&iter, &len, &data);
      iov [i].iov_base = (void *)data;
      iov [i].iov_len = len;
      i++;
   }

   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 0;
   rpc.insert.response_to = 0;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags =
      (command->u.insert.ordered ? 0 : MONGOC_INSERT_CONTINUE_ON_ERROR);
   rpc.insert.collection = ns;
   rpc.insert.documents = iov;
   rpc.insert.n_documents = command->u.insert.n_documents;

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_has_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         GOTO (cleanup);
      }
   }

   ret = true;

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, gle);
      bson_destroy (gle);
   }

   bson_free (iov);

   RETURN (ret);
}


static bool
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
   bson_t *gle;
   size_t err_offset;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   bool ret = false;

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
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "update document is corrupt or contains "
                      "invalid keys including $ or .");
      RETURN (false);
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
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_has_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         GOTO (cleanup);
      }
   }

   ret = true;

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, gle);
      bson_destroy (gle);
   }

   RETURN (ret);
}


static bool
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

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

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

   bson_destroy (&reply);
   bson_destroy (&cmd);

   return ret;
}


static bool
_mongoc_write_command_insert (mongoc_write_command_t       *command,
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
   bool ret;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   BSON_APPEND_UTF8 (&cmd, "insert", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.insert.ordered);
   BSON_APPEND_ARRAY (&cmd, "documents", command->u.insert.documents);

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   _mongoc_write_result_merge (result, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   return ret;
}


static bool
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

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

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

   _mongoc_write_result_merge (result, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   return ret;
}


static mongoc_write_op_t gWriteOps [2][3] = {
   { _mongoc_write_command_delete_legacy,
     _mongoc_write_command_insert_legacy,
     _mongoc_write_command_update_legacy },
   { _mongoc_write_command_delete,
     _mongoc_write_command_insert,
     _mongoc_write_command_update },
};


bool
_mongoc_write_command_execute (mongoc_write_command_t       *command,       /* IN */
                               mongoc_client_t              *client,        /* IN */
                               uint32_t                      hint,          /* IN */
                               const char                   *database,      /* IN */
                               const char                   *collection,    /* IN */
                               const mongoc_write_concern_t *write_concern, /* IN */
                               mongoc_write_result_t        *result,        /* OUT */
                               bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *node;
   bool ret = false;
   int mode = 0;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (collection);

   if (!hint) {
      hint = _mongoc_client_preselect (client, MONGOC_OPCODE_INSERT,
                                       write_concern, NULL, error);
      if (!hint) {
         RETURN (false);
      }
   }

   node = &client->cluster.nodes [hint - 1];
   mode = SUPPORTS_WRITE_COMMANDS (node);

   ret = gWriteOps [mode][command->type] (command, client, hint, database,
                                          collection, write_concern, result,
                                          error);

   RETURN (ret);
}


void
_mongoc_write_command_destroy (mongoc_write_command_t *command)
{
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
}


void
_mongoc_write_result_init (mongoc_write_result_t *result) /* IN */
{
   BSON_ASSERT (result);

   memset (result, 0, sizeof *result);

   bson_init (&result->upserted);
   bson_init (&result->write_concern_errors);
   bson_init (&result->write_errors);
}


void
_mongoc_write_result_destroy (mongoc_write_result_t *result)
{
   BSON_ASSERT (result);

   bson_destroy (&result->upserted);
   bson_destroy (&result->write_concern_errors);
   bson_destroy (&result->write_errors);
}


void
_mongoc_write_result_merge_legacy (mongoc_write_result_t *result, /* IN */
                                   const bson_t          *reply)  /* IN */
{
   BSON_ASSERT (result);
   BSON_ASSERT (reply);

   result->omit_nModified = true;
}


void
_mongoc_write_result_merge (mongoc_write_result_t *result, /* IN */
                            const bson_t          *reply)  /* IN */
{
   bson_iter_t iter;
   int32_t v32;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

#define UPDATE_FIELD(name) \
   if (bson_iter_init_find (&iter, reply, #name) && \
       BSON_ITER_HOLDS_INT32 (&iter)) { \
      v32 = bson_iter_int32 (&iter); \
      result->name += v32; \
   }

   UPDATE_FIELD (nInserted);
   UPDATE_FIELD (nMatched);
   UPDATE_FIELD (nModified);
   UPDATE_FIELD (nRemoved);
   UPDATE_FIELD (nUpserted);

   if (!bson_has_field (reply, "nModified")) {
      result->omit_nModified = true;
   }

#undef UPDATE_FIELD
}


void
_mongoc_write_result_to_bson (mongoc_write_result_t *result,
                              bson_t                *bson)
{
   BSON_ASSERT (result);

   if (bson) {
      bson_init (bson);

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
      if (!bson_empty0 (&result->write_errors)) {
         BSON_APPEND_ARRAY (bson, "writeErrors", &result->write_errors);
      }
      if (!bson_empty0 (&result->write_concern_errors)) {
         BSON_APPEND_ARRAY (bson, "writeConcernErrors",
                            &result->write_concern_errors);
      }
   }
}
