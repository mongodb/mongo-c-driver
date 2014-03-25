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


bool
mongoc_bulk_operation_execute (mongoc_bulk_operation_t      *bulk,          /* IN */
                               bson_t                       *reply,         /* OUT */
                               bson_error_t                 *error)         /* OUT */
{
   return false;
}
