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


#include "mongoc-bulk-operation.h"
#include "mongoc-bulk-operation-private.h"
#include "mongoc-client-private.h"
#include "mongoc-error.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"
#include "mongoc-write-command-private.h"
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

   _mongoc_array_init (&bulk->commands, sizeof (mongoc_write_command_t));

   return bulk;
}


void
mongoc_bulk_operation_destroy (mongoc_bulk_operation_t *bulk) /* IN */
{
   mongoc_write_command_t *command;
   int i;

   if (bulk) {
      for (i = 0; i < bulk->commands.len; i++) {
         command = &_mongoc_array_index (&bulk->commands,
                                         mongoc_write_command_t, i);
         _mongoc_write_command_destroy (command);
      }

      bson_free (bulk->database);
      bson_free (bulk->collection);
      _mongoc_array_destroy (&bulk->commands);
      mongoc_write_concern_destroy (bulk->write_concern);
      bson_clear (&bulk->upserted);
      bson_clear (&bulk->write_errors);
      bson_clear (&bulk->write_concern_errors);

      bson_free (bulk);
   }
}


void
mongoc_bulk_operation_delete (mongoc_bulk_operation_t *bulk,     /* IN */
                              const bson_t            *selector) /* IN */
{
   mongoc_write_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   _mongoc_write_command_init_delete (&command, selector, true, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_delete_one (mongoc_bulk_operation_t *bulk,     /* IN */
                                  const bson_t            *selector) /* IN */
{
   mongoc_write_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   _mongoc_write_command_init_delete (&command, selector, false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_insert (mongoc_bulk_operation_t *bulk,
                              const bson_t            *document)
{
   mongoc_write_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (document);

   _mongoc_write_command_init_insert (&command, &document, 1, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_replace_one (mongoc_bulk_operation_t *bulk,
                                   const bson_t            *selector,
                                   const bson_t            *document,
                                   bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
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

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector,
                              const bson_t            *document,
                              bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
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

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      true, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector,
                                  const bson_t            *document,
                                  bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
   bson_iter_t iter;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update_one only works with $ operators.",
                            __FUNCTION__);
            return;
         }
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


static void
_mongoc_bulk_operation_do_append_upserted (mongoc_bulk_operation_t *bulk,
                                           uint32_t                 idx,
                                           const bson_value_t      *_id)
{
   const char *key = NULL;
   uint32_t count;
   bson_t child;
   char str [16];

   BSON_ASSERT (bulk);
   BSON_ASSERT (_id);

   if (!bulk->upserted) {
      bulk->upserted = bson_new ();
   }

   count = bson_count_keys (bulk->upserted);
   bson_uint32_to_string (count, &key, str, sizeof str);

   bson_append_document_begin (bulk->upserted, key, -1, &child);
   BSON_APPEND_INT32 (&child, "index", bulk->offset + idx);
   BSON_APPEND_VALUE (&child, "_id", _id);
   bson_append_document_end (bulk->upserted, &child);
}


static void
_mongoc_bulk_operation_append_upserted (mongoc_bulk_operation_t *bulk, /* IN */
                                        const bson_iter_t       *iter) /* IN */
{
   const bson_value_t *vptr;
   bson_value_t value = { 0 };
   bson_iter_t citer;
   int idx = 0;

   BSON_ASSERT (bulk);
   BSON_ASSERT (iter);

   if (!BSON_ITER_HOLDS_DOCUMENT (iter) ||
       !bson_iter_recurse (iter, &citer)) {
      return;
   }

   while (bson_iter_next (&citer)) {
      if (BSON_ITER_IS_KEY (&citer, "index") &&
          BSON_ITER_HOLDS_INT32 (&citer)) {
         idx = bson_iter_int32 (&citer);
      } else if (BSON_ITER_IS_KEY (&citer, "_id")) {
         if ((vptr = bson_iter_value (&citer))) {
            bson_value_copy (vptr, &value);
         }
      }
   }

   if (value.value_type) {
      _mongoc_bulk_operation_do_append_upserted (bulk, idx, &value);
      bson_value_destroy (&value);
   }
}


bool
mongoc_bulk_operation_execute (mongoc_bulk_operation_t *bulk,  /* IN */
                               bson_t                  *reply, /* OUT */
                               bson_error_t            *error) /* OUT */
{
   mongoc_write_command_t *command;
   bson_t local_reply;
   uint32_t hint;
   bool ret = false;
   int i;

   ENTRY;

   bson_return_val_if_fail (bulk, false);

   bson_init (reply);

   if (!bulk->commands.len) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Cannot do an empty bulk write");
      RETURN (false);
   }

   hint = _mongoc_client_preselect (bulk->client,
                                    MONGOC_OPCODE_INSERT,
                                    bulk->write_concern,
                                    NULL,
                                    error);
   if (!hint) {
      RETURN (false);
   }

   for (i = 0; i < bulk->commands.len; i++) {
      command = &_mongoc_array_index (&bulk->commands,
                                      mongoc_write_command_t, i);

      ret = _mongoc_write_command_execute (command, bulk->client, hint,
                                           bulk->database, bulk->collection,
                                           bulk->write_concern, &local_reply,
                                           error);

      bson_destroy (&local_reply);

      if (!ret && bulk->ordered) {
         GOTO (cleanup);
      }
   }

   ret = true;

cleanup:

   RETURN (ret);
}
