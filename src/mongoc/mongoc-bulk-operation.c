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
#include "mongoc-error.h"
#include "mongoc-trace.h"
#include "mongoc-write-concern-private.h"


mongoc_bulk_operation_t *
_mongoc_bulk_operation_new (mongoc_client_t              *client,        /* IN */
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
   bulk->collection = bson_strdup (collection);
   bulk->ordered = ordered;
   bulk->hint = hint;
   bulk->write_concern = mongoc_write_concern_copy (write_concern);

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
         case MONGOC_BULK_COMMAND_REPLACE:
            bson_destroy (c->u.replace.selector);
            bson_destroy (c->u.replace.document);
            break;
         default:
            BSON_ASSERT (false);
            break;
         }
      }

      bson_free (bulk->collection);
      _mongoc_array_destroy (&bulk->commands);
      mongoc_write_concern_destroy (bulk->write_concern);

      bson_free (bulk);
   }
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

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   command.type = MONGOC_BULK_COMMAND_REPLACE;
   command.u.replace.upsert = upsert;
   command.u.replace.selector = bson_copy (selector);
   command.u.replace.document = bson_copy (document);

   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector,
                              const bson_t            *document,
                              bool                     upsert)
{
   mongoc_bulk_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   command.type = MONGOC_BULK_COMMAND_REPLACE;
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
   case MONGOC_BULK_COMMAND_REPLACE:
      BSON_APPEND_UTF8 (bson, "update", bulk->collection);
      BSON_APPEND_DOCUMENT (bson, "writeConcern", wc);
      BSON_APPEND_BOOL (bson, "ordered", bulk->ordered);
      break;
   default:
      BSON_ASSERT (false);
      break;
   }

   EXIT;
}


bool
mongoc_bulk_operation_execute (mongoc_bulk_operation_t *bulk,  /* IN */
                               bson_t                  *reply, /* OUT */
                               bson_error_t            *error) /* OUT */
{
   mongoc_bulk_command_t *c;
   bson_t command;
   bool ret = false;
   int i;

   ENTRY;

   bson_return_val_if_fail (bulk, false);

   if (!bulk->commands.len) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Cannot do an empty bulk write");
   }

   for (i = 0; i < bulk->commands.len; i++) {
      c = &_mongoc_array_index (&bulk->commands, mongoc_bulk_command_t, i);
      _mongoc_bulk_operation_build (bulk, c, &command);
      bson_destroy (&command);
   }

   RETURN (ret);
}
