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

#include "mongoc-host-list.h"
#include "mongoc-server-description.h"
#include "mongoc-trace.h"
#include "mongoc-uri.h"

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_init --
 *
 *       Initialize a new server_description_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_server_description_init (mongoc_server_description_t *description,
                                 const char                  *address,
                                 int32_t                     id)
{
   mongoc_host_list_t host;

   ENTRY;

   bson_return_if_fail(description);
   bson_return_if_fail(address);

   memset (description, 0, sizeof *description);

   description->id = id;
   description->next = NULL;
   description->set_name = NULL;
   description->connection_address = bson_strdup(address);
   description->error = NULL; // TODO change if this changes types
   description->round_trip_time = -1; // TODO: different default?
   description->type = MONGOC_SERVER_TYPE_UNKNOWN;
   description->min_wire_version = -1; // TODO: different default?
   description->min_wire_version = -1; // TODO: different default?

   description->rs_members = NULL; // TODO: proper way to initialize?
   description->rs_members_len = 0;

   description->host = host;
   if (!_mongoc_host_list_from_string(&description->host, address)) {
      MONGOC_WARNING("Failed to parse uri for %s", address);
      return;
   }

   bson_init(&description->tags);
   description->current_primary = NULL;
   description->max_write_batch_size = -1; // TODO: different default?

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_destroy --
 *
 *       Destroy allocated resources within @description.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_server_description_destroy (mongoc_server_description_t *description)
{
   ENTRY;

   BSON_ASSERT(description);

   if (description->tags.len) {
      bson_init(&description->tags);
      memset (&description->tags, 0, sizeof description->tags);
   }

   bson_free(description->connection_address);

   if (description->set_name) {
      bson_free(description->set_name);
      description->set_name = NULL;
   }

   if (description->current_primary) {
      bson_free(description->current_primary);
      description->current_primary = NULL;
   }

   EXIT;
}
