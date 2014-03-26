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


#include "mongoc-write-command-private.h"
#include "mongoc-write-concern-private.h"


static void
_add_write_concern (bson_t                       *command,
                    const mongoc_write_concern_t *write_concern)
{
   const bson_t *wc;

   if (write_concern) {
      wc = _mongoc_write_concern_freeze ((void *)write_concern);
      if (wc) {
         BSON_APPEND_DOCUMENT (command, "writeConcern", wc);
      }
   }
}


void
_mongoc_write_command_insert (bson_t                       *command,
                              const char                   *collection,
                              bool                          ordered,
                              const bson_t                 *document,
                              const mongoc_write_concern_t *write_concern)
{
   bson_t ar;

   BSON_ASSERT (command);
   BSON_ASSERT (collection);
   BSON_ASSERT (document);

   bson_init (command);
   BSON_APPEND_UTF8 (command, "insert", collection);
   _add_write_concern (command, write_concern);
   BSON_APPEND_BOOL (command, "ordered", ordered);
   bson_append_array_begin (command, "documents", 9, &ar);
   BSON_APPEND_DOCUMENT (&ar, "0", document);
   bson_append_array_end (command, &ar);
}


void
_mongoc_write_command_update (bson_t                       *command,
                              const char                   *collection,
                              const bson_t                 *selector,
                              const bson_t                 *document,
                              bool                          ordered,
                              bool                          multi,
                              bool                          upsert,
                              const mongoc_write_concern_t *write_concern)
{
   bson_t ar;
   bson_t child;

   BSON_ASSERT (command);
   BSON_ASSERT (collection);
   BSON_ASSERT (selector);
   BSON_ASSERT (document);

   bson_init (command);
   BSON_APPEND_UTF8 (command, "update", collection);
   _add_write_concern (command, write_concern);
   BSON_APPEND_BOOL (command, "ordered", ordered);
   bson_append_array_begin (command, "updates", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", selector);
   BSON_APPEND_DOCUMENT (&child, "u", document);
   BSON_APPEND_BOOL (&child, "multi", multi);
   BSON_APPEND_BOOL (&child, "upsert", upsert);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (command, &ar);
}


void
_mongoc_write_command_delete (bson_t                       *command,
                              const char                   *collection,
                              const bson_t                 *selector,
                              bool                          ordered,
                              bool                          multi,
                              const mongoc_write_concern_t *write_concern)
{
   bson_t ar;
   bson_t child;

   BSON_ASSERT (command);
   BSON_ASSERT (collection);
   BSON_ASSERT (selector);

   bson_init (command);
   BSON_APPEND_UTF8 (command, "delete", collection);
   _add_write_concern (command, write_concern);
   BSON_APPEND_BOOL (command, "ordered", ordered);
   bson_append_array_begin (command, "deletes", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", selector);
   BSON_APPEND_INT32 (&child, "limit", multi ? 0 : 1);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (command, &ar);
}
