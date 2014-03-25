/*inserted = value;
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


#include "mongoc-bulk-operation.h"
#include "mongoc-bulk-operation-private.h"
#include "mongoc-client-private.h"
#include "mongoc-error.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"
#include "mongoc-write-concern-private.h"


/*
 * This is the implementation of both write commands and bulk write commands.
 * They are all implemented as one contiguous set since we'd like to cut down
 * on code duplication here.
 *
 * This implementation is currently naive.
 *
 * Some interesting optimizations might be:
 *
 *   - If unordered mode, send operations as we get them instead of waiting
 *     for execute() to be called. This could save us memcpy()'s too.
 *   - If there is no acknowledgement desired, keep a count of how many
 *     replies we need and ask the socket layer to skip that many bytes
 *     when reading.
 *   - Try to use iovec to send write commands with subdocuments rather than
 *     copying them into the write command document.
 */


mongoc_bulk_operation_t *
_mongoc_bulk_operation_new (mongoc_client_t              *client,        /* IN */
                            const char                   *database,      /* IN */
                            const char                   *collection,    /* IN */
                            uint32_t                      hint,          /* IN */
                            bool                          ordered,       /* IN */
                            const mongoc_write_concern_t *write_concern) /* IN */
{
   mongoc_bulk_operation_t *bulk;

   BSON_ASSERT (client);
   BSON_ASSERT (collection);

   bulk = bson_malloc0 (sizeof *bulk);

   bulk->client = client;
   bulk->database = bson_strdup (database);
   bulk->collection = bson_strdup (collection);
   bulk->ordered = ordered;
   bulk->hint = hint;
   bulk->write_concern = mongoc_write_concern_copy (write_concern);

   if (!bulk->write_concern) {
      bulk->write_concern = mongoc_write_concern_new ();
   }

   _mongoc_array_init (&bulk->commands, sizeof (mongoc_bulk_command_t));

   return bulk;
}


void
mongoc_bulk_operation_destroy (mongoc_bulk_operation_t *bulk) /* IN */
{
   if (bulk) {
      mongoc_bulk_command_t *c;
      int i;

      for (i = 0; i < bulk->commands.len; i++) {
         c = &_mongoc_array_index (&bulk->commands, mongoc_bulk_command_t, i);
         switch (c->type) {
         case MONGOC_BULK_COMMAND_INSERT:
            bson_destroy (c->u.insert.document);
            break;
         case MONGOC_BULK_COMMAND_UPDATE:
            bson_destroy (c->u.update.selector);
            bson_destroy (c->u.update.document);
            break;
         case MONGOC_BULK_COMMAND_DELETE:
            bson_destroy (c->u.delete.selector);
            break;
         default:
            BSON_ASSERT (false);
            break;
         }
      }

      bson_free (bulk->database);
      bson_free (bulk->collection);

      _mongoc_array_destroy (&bulk->commands);

      mongoc_write_concern_destroy (bulk->write_concern);

      if (bulk->upserted) {
         bson_destroy (bulk->upserted);
      }

      if (bulk->write_errors) {
         bson_destroy (bulk->write_errors);
      }

      if (bulk->write_concern_errors) {
         bson_destroy (bulk->write_concern_errors);
      }

      bson_free (bulk);
   }
}


static uint32_t
_mongoc_bulk_operation_preselect (mongoc_bulk_operation_t *bulk,
                                  uint32_t                *min_wire_version,
                                  uint32_t                *max_wire_version,
                                  bson_error_t            *error)
{
   uint32_t hint;

   BSON_ASSERT (bulk);
   BSON_ASSERT (min_wire_version);
   BSON_ASSERT (max_wire_version);

   *min_wire_version = 0;
   *max_wire_version = 0;

   hint = _mongoc_client_preselect (bulk->client,
                                    MONGOC_OPCODE_INSERT,
                                    bulk->write_concern,
                                    NULL,
                                    error);

   if (hint) {
      *min_wire_version = bulk->client->cluster.nodes [hint].min_wire_version;
      *max_wire_version = bulk->client->cluster.nodes [hint].max_wire_version;
   }

   return hint;
}


void
mongoc_bulk_operation_delete (mongoc_bulk_operation_t *bulk,     /* IN */
                              const bson_t            *selector) /* IN */
{
   mongoc_bulk_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   command.type = MONGOC_BULK_COMMAND_DELETE;
   command.u.delete.multi = true;
   command.u.delete.selector = bson_copy (selector);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_delete_one (mongoc_bulk_operation_t *bulk,     /* IN */
                                  const bson_t            *selector) /* IN */
{
   mongoc_bulk_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   command.type = MONGOC_BULK_COMMAND_DELETE;
   command.u.delete.multi = false;
   command.u.delete.selector = bson_copy (selector);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_insert (mongoc_bulk_operation_t *bulk,
                              const bson_t            *document)
{
   mongoc_bulk_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (document);

   command.type = MONGOC_BULK_COMMAND_INSERT;
   command.u.insert.document = bson_copy (document);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_replace_one (mongoc_bulk_operation_t *bulk,
                                   const bson_t            *selector,
                                   const bson_t            *document,
                                   bool                     upsert)
{
   mongoc_bulk_command_t command = { 0 };
   size_t err_off;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (!bson_validate (document,
                       (BSON_VALIDATE_DOT_KEYS | BSON_VALIDATE_DOLLAR_KEYS),
                       &err_off)) {
      MONGOC_WARNING ("%s(): replacement document may not contain "
                      "$ or . in keys. Ingoring document.",
                      __FUNCTION__);
      return;
   }

   command.type = MONGOC_BULK_COMMAND_UPDATE;
   command.u.update.upsert = upsert;
   command.u.update.multi = false;
   command.u.update.selector = bson_copy (selector);
   command.u.update.document = bson_copy (document);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector,
                              const bson_t            *document,
                              bool                     upsert)
{
   mongoc_bulk_command_t command = { 0 };
   bson_iter_t iter;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update only works with $ operators.",
                            __FUNCTION__);
            return;
         }
      }
   }

   command.type = MONGOC_BULK_COMMAND_UPDATE;
   command.u.update.upsert = upsert;
   command.u.update.multi = true;
   command.u.update.selector = bson_copy (selector);
   command.u.update.document = bson_copy (document);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector,
                                  const bson_t            *document,
                                  bool                     upsert)
{
   mongoc_bulk_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   command.type = MONGOC_BULK_COMMAND_UPDATE;
   command.u.update.upsert = upsert;
   command.u.update.multi = false;
   command.u.update.selector = bson_copy (selector);
   command.u.update.document = bson_copy (document);

   _mongoc_array_append_val (&bulk->commands, command);
}


static void
_mongoc_bulk_operation_build (mongoc_bulk_operation_t *bulk,    /* IN */
                              mongoc_bulk_command_t   *command, /* IN */
                              bson_t                  *bson)    /* OUT */
{
   const bson_t *wc;
   bson_t ar;
   bson_t child;

   ENTRY;

   bson_return_if_fail (bulk);
   bson_return_if_fail (command);
   bson_return_if_fail (bson);

   wc = _mongoc_write_concern_freeze ((void *)bulk->write_concern);

   bson_init (bson);

   /*
    * TODO: Allow insert to be an array of documents.
    */

   switch (command->type) {
   case MONGOC_BULK_COMMAND_INSERT:
      BSON_APPEND_UTF8 (bson, "insert", bulk->collection);
      BSON_APPEND_DOCUMENT (bson, "writeConcern", wc);
      BSON_APPEND_BOOL (bson, "ordered", bulk->ordered);
      bson_append_array_begin (bson, "documents", 9, &ar);
      BSON_APPEND_DOCUMENT (&ar, "0", command->u.insert.document);
      bson_append_array_end (bson, &ar);
      break;
   case MONGOC_BULK_COMMAND_UPDATE:
      BSON_APPEND_UTF8 (bson, "update", bulk->collection);
      BSON_APPEND_DOCUMENT (bson, "writeConcern", wc);
      BSON_APPEND_BOOL (bson, "ordered", bulk->ordered);
      bson_append_array_begin (bson, "updates", 7, &ar);
      bson_append_document_begin (&ar, "0", 1, &child);
      BSON_APPEND_DOCUMENT (&child, "q", command->u.update.selector);
      BSON_APPEND_DOCUMENT (&child, "u", command->u.update.document);
      BSON_APPEND_BOOL (&child, "multi", command->u.update.multi);
      BSON_APPEND_BOOL (&child, "upsert", command->u.update.upsert);
      bson_append_document_end (&ar, &child);
      bson_append_array_end (bson, &ar);
      break;
   case MONGOC_BULK_COMMAND_DELETE:
      BSON_APPEND_UTF8 (bson, "delete", bulk->collection);
      BSON_APPEND_DOCUMENT (bson, "writeConcern", wc);
      BSON_APPEND_BOOL (bson, "ordered", bulk->ordered);
      bson_append_array_begin (bson, "deletes", 7, &ar);
      bson_append_document_begin (&ar, "0", 1, &child);
      BSON_APPEND_DOCUMENT (&child, "q", command->u.delete.selector);
      BSON_APPEND_INT32 (&child, "limit", command->u.delete.multi ? 0 : 1);
      bson_append_document_end (&ar, &child);
      bson_append_array_end (bson, &ar);
      break;
   default:
      BSON_ASSERT (false);
      break;
   }

   EXIT;
}


static bool
_mongoc_bulk_operation_send (mongoc_bulk_operation_t *bulk,    /* IN */
                             const bson_t            *command, /* IN */
                             bson_t                  *reply,   /* OUT */
                             bson_error_t            *error)
{
   mongoc_read_prefs_t *read_prefs;
   bool ret;

   BSON_ASSERT (bulk);
   BSON_ASSERT (command);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   ret = mongoc_client_command_simple (bulk->client,
                                       bulk->database,
                                       command,
                                       read_prefs,
                                       reply,
                                       error);

   mongoc_read_prefs_destroy (read_prefs);

   return ret;
}


static bool
_mongoc_bulk_operation_send_legacy (mongoc_bulk_operation_t *bulk,       /* IN */
                                    mongoc_collection_t     *collection, /* IN */
                                    mongoc_bulk_command_t   *command,    /* IN */
                                    bson_t                  *reply,      /* OUT */
                                    bson_error_t            *error)      /* OUT */
{
   const bson_t *gle;
   bson_iter_t iter;
   int32_t n = -1;
   bool ret = false;
   int flags;

   BSON_ASSERT (bulk);
   BSON_ASSERT (command);
   BSON_ASSERT (reply);

   bson_init (reply);

   switch (command->type) {
   case MONGOC_BULK_COMMAND_DELETE:
      flags = command->u.delete.multi ? MONGOC_DELETE_NONE
                                      : MONGOC_DELETE_SINGLE_REMOVE;
      ret = mongoc_collection_delete (collection,
                                      (mongoc_delete_flags_t)flags,
                                      command->u.delete.selector,
                                      bulk->write_concern,
                                      error);
      break;
   case MONGOC_BULK_COMMAND_INSERT:
      flags = bulk->ordered ? MONGOC_INSERT_NONE
                            : MONGOC_INSERT_CONTINUE_ON_ERROR;
      ret = mongoc_collection_insert (collection,
                                      (mongoc_insert_flags_t)flags,
                                      command->u.insert.document,
                                      bulk->write_concern,
                                      error);
      break;
   case MONGOC_BULK_COMMAND_UPDATE:
      bulk->omit_n_modified = true;
      flags = 0;
      flags |= command->u.update.multi ? MONGOC_UPDATE_MULTI_UPDATE : 0;
      flags |= command->u.update.upsert ? MONGOC_UPDATE_UPSERT : 0;
      ret = mongoc_collection_update (collection,
                                      (mongoc_update_flags_t)flags,
                                      command->u.update.selector,
                                      command->u.update.document,
                                      bulk->write_concern,
                                      error);
      break;
   default:
      BSON_ASSERT (false);
      break;
   }

   gle = mongoc_collection_get_last_error (collection);

   if (gle) {
      if (bson_iter_init_find (&iter, gle, "n") &&
          BSON_ITER_HOLDS_INT32 (&iter)) {
         n = bson_iter_int32 (&iter);
      }

      switch (command->type) {
      case MONGOC_BULK_COMMAND_DELETE:
         if (n > 0) {
            bulk->n_removed += n;
         }
         break;
      case MONGOC_BULK_COMMAND_INSERT:
         if (n > 0) {
            bulk->n_inserted += n;
         } else if (ret) {
            bulk->n_inserted += 1;
         }
         break;
      case MONGOC_BULK_COMMAND_UPDATE:
         if (bson_iter_init_find (&iter, gle, "upserted") &&
             BSON_ITER_HOLDS_ARRAY (&iter)) {
            bulk->n_upserted += n;
         } else {
            bulk->n_matched += n;
         }
         break;
      default:
         BSON_ASSERT (false);
         break;
      }
   }

   return ret;
}


static void
_mongoc_bulk_operation_append_upserted (mongoc_bulk_operation_t *bulk, /* IN */
                                        const bson_iter_t       *iter) /* IN */
{
   BSON_ASSERT (bulk);
   BSON_ASSERT (iter);

   if (!bulk->upserted) {
      bulk->upserted = bson_new ();
   }
}


static void
_mongoc_bulk_operation_process_reply (mongoc_bulk_operation_t *bulk,   /* IN */
                                      int                      type,   /* IN */
                                      const bson_t            *reply)  /* IN */
{
   bson_iter_t iter;
   bson_iter_t ar;
   int32_t n = 0;
   int32_t n_upserted = 0;

   BSON_ASSERT (bulk);
   BSON_ASSERT (reply);

   if (bson_iter_init_find (&iter, reply, "n") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      n = bson_iter_int32 (&iter);

      switch (type) {
      case MONGOC_BULK_COMMAND_DELETE:
         bulk->n_removed += n;
         break;
      case MONGOC_BULK_COMMAND_INSERT:
         bulk->n_inserted += n;
         break;
      case MONGOC_BULK_COMMAND_UPDATE:
         if (bson_iter_init_find (&iter, reply, "upserted")) {
            if (BSON_ITER_HOLDS_ARRAY (&iter) &&
                bson_iter_recurse (&iter, &ar)) {
               while (bson_iter_next (&ar)) {
                  _mongoc_bulk_operation_append_upserted (bulk, &ar);
                  n_upserted++;
               }
            } else {
               n_upserted = 1;
            }
            bulk->n_upserted += n_upserted;
            bulk->n_matched += (n - n_upserted);
         } else {
            bulk->n_matched += n;
         }

         /*
          * In a mixed sharded cluster, a call to update() could return
          * nModified (>= 2.6) or not (<= 2.4). If any call does not return
          * nModified we can't report a valid final count so omit the field
          * completely from the result.
          *
          * See SERVER-13001 for more information.
          */
         if (!bson_iter_init_find (&iter, reply, "nModified")) {
            bulk->omit_n_modified = true;
         } else {
            bulk->n_modified += bson_iter_int32 (&iter);
         }

         break;
      default:
         BSON_ASSERT (false);
         break;
      }
   }
}


static void
_mongoc_bulk_operation_build_reply (mongoc_bulk_operation_t *bulk,  /* IN */
                                    bson_t                  *reply) /* IN */
{
   BSON_ASSERT (bulk);

   if (reply) {
      if (!bulk->omit_n_modified) {
         BSON_APPEND_INT32 (reply, "nModified", bulk->n_modified);
      }
      BSON_APPEND_INT32 (reply, "nUpserted", bulk->n_upserted);
      BSON_APPEND_INT32 (reply, "nMatched", bulk->n_matched);
      BSON_APPEND_INT32 (reply, "nRemoved", bulk->n_removed);
      BSON_APPEND_INT32 (reply, "nInserted", bulk->n_inserted);
#if 0
      BSON_APPEND_DOCUMENT (reply, "writeErrors", bulk->writeErrors);
      BSON_APPEND_DOCUMENT (reply, "writeConcernErrors", bulk->upserted);
      BSON_APPEND_DOCUMENT (reply, "upserted", bulk->upserted);
#endif
   }
}


bool
mongoc_bulk_operation_execute (mongoc_bulk_operation_t *bulk,  /* IN */
                               bson_t                  *reply, /* OUT */
                               bson_error_t            *error) /* OUT */
{
   mongoc_bulk_command_t *c;
   mongoc_collection_t *collection = NULL;
   uint32_t min_wire_version;
   uint32_t max_wire_version;
   bson_t command;
   bson_t local_reply;
   bool ret = false;
   int i;

   ENTRY;

   bson_return_val_if_fail (bulk, false);

   bson_init (reply);

   if (!_mongoc_bulk_operation_preselect (bulk,
                                          &min_wire_version,
                                          &max_wire_version,
                                          error)) {
      RETURN (false);
   }

   if (!bulk->commands.len) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Cannot do an empty bulk write");
      RETURN (false);
   }

   if (!collection) {
      collection = mongoc_client_get_collection (bulk->client,
                                                 bulk->database,
                                                 bulk->collection);
   }

   for (i = 0; i < bulk->commands.len; i++) {
      c = &_mongoc_array_index (&bulk->commands, mongoc_bulk_command_t, i);

      if (max_wire_version >= 2) {
         _mongoc_bulk_operation_build (bulk, c, &command);
         ret = _mongoc_bulk_operation_send (bulk, &command, &local_reply,
                                            error);
         bson_destroy (&command);
      } else {
         ret = _mongoc_bulk_operation_send_legacy (bulk, collection, c,
                                                   &local_reply, error);
      }

      _mongoc_bulk_operation_process_reply (bulk, c->type, &local_reply);
      bson_destroy (&local_reply);

      if (!ret && bulk->ordered) {
         _mongoc_bulk_operation_build_reply (bulk, reply);
         GOTO (cleanup);
      }
   }

   ret = true;

cleanup:
   _mongoc_bulk_operation_build_reply (bulk, reply);

   RETURN (ret);
}
