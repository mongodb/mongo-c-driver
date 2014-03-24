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
_mongoc_bulk_operation_new (mongoc_client_t *client,     /* IN */
                            const char      *collection, /* IN */
                            uint32_t         hint,       /* IN */
                            bool             ordered)    /* IN */
{
   mongoc_bulk_operation_t *bulk;

   BSON_ASSERT (client);
   BSON_ASSERT (collection);

   bulk = bson_malloc0 (sizeof *bulk);

   bulk->client = client;
   bulk->collection = bson_strdup (collection);
   bulk->ordered = ordered;
   bulk->hint = hint;

   bson_init (&bulk->command);

   return bulk;
}


void
mongoc_bulk_operation_destroy (mongoc_bulk_operation_t *bulk) /* IN */
{
   if (bulk) {
      bson_free (bulk->collection);
      bson_destroy (&bulk->command);
      bson_free (bulk);
   }
}
