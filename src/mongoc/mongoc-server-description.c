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
#include "mongoc-host-list-private.h"
#include "mongoc-server-description.h"
#include "mongoc-trace.h"
#include "mongoc-uri.h"

#define MIN_WIRE_VERSION 0
#define MAX_WIRE_VERSION 3

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
   description->error = NULL; // TODO SDAM change if this changes types
   description->type = MONGOC_SERVER_UNKNOWN;

   description->round_trip_time = -1;
   description->min_wire_version = MIN_WIRE_VERSION;
   description->max_wire_version = MAX_WIRE_VERSION;

   description->rs_members = NULL;

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
   char **member_iter;

   ENTRY;

   BSON_ASSERT(description);

   if (description->tags.len) {
      bson_init(&description->tags);
      memset (&description->tags, 0, sizeof description->tags);
   }

   bson_free(description->connection_address);

   member_iter = description->rs_members;
   while (member_iter) {
      free(*member_iter);
      member_iter++;
   }
   free(description->rs_members);
   description->rs_members = NULL;

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

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_has_rs_member --
 *
 *       Return true if this address is included in server's list of rs
 *       members, false otherwise.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_server_description_has_rs_member(mongoc_server_description_t *server,
                                         const char                  *address)
{
   char **member_iter = server->rs_members;

   while (member_iter) {
      if (*member_iter == address) {
         return true;
      }
      member_iter++;
   }
   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_server_description_set_state --
 *
 *       Change the state of this server.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */

void _mongoc_server_description_set_state (mongoc_server_description_t *description,
                                           mongoc_server_description_type_t type)
{
   description->type = type;
   // TODO SDAM unblock waiters? Can waiters wait on particular server?
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_server_description_update_rtt --
 *
 *       Update this server's rtt calculation using an exponentially-
 *       weighted moving average formula.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes this server description's rtt.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_server_description_update_rtt (mongoc_server_description_t *server,
                                       int64_t new_time)
{
   // TODO SS implement
   return;
}
