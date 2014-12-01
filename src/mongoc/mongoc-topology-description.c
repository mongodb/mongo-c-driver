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

#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"
#include "mongoc-trace.h"

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_init --
 *
 *       Initialize the given topology description
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
_mongoc_topology_description_init (mongoc_topology_description_t *description)
{
   ENTRY;

   bson_return_if_fail(description);

   description->type = MONGOC_CLUSTER_UNKNOWN;
   description->servers = NULL;
   description->set_name = NULL;
   description->compatible = true; // TODO: different default?
   description->compatibility_error = NULL;

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_destroy --
 *
 *       Destroy allocated resources within @description
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
_mongoc_topology_description_destroy (mongoc_topology_description_t *description)
{
   mongoc_server_description_t *prev_server = NULL;
   mongoc_server_description_t *server_iter;

   ENTRY;

   BSON_ASSERT(description);

   // TODO SDAM unblock waiters?

   server_iter = description->servers;
   while (server_iter) {
      if (prev_server) {
         _mongoc_server_description_destroy(prev_server);
         // TODO SDAM: need to free this?
         prev_server = NULL;
      }
      prev_server = server_iter;
      server_iter = server_iter->next;
   }

   if (description->set_name) {
      bson_free (description->set_name);
   }

   if (description->compatibility_error) {
      bson_free (description->compatibility_error);
   }

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_remove_server --
 *
 *       If present, remove this server from this topology description.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Removes server from topology's list of servers.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_topology_description_remove_server (mongoc_topology_description_t *description,
                                            mongoc_server_description_t   *server)
{
   mongoc_server_description_t *server_iter = description->servers;
   mongoc_server_description_t *previous_server = NULL;

   while (server_iter) {
      if (server_iter->connection_address == server->connection_address) {
         if (previous_server) {
            previous_server->next = server_iter->next;
         } else {
            description->servers = server_iter->next;
         }
         break;
      }
      previous_server = server_iter;
      server_iter = server_iter->next;
   }
   // TODO: some sort of callback to clusters?
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_topology_has_server --
 *
 *       Return true if the given server is in the given topology,
 *       false otherwise.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_topology_description_has_server (mongoc_topology_description_t *description,
                                         const char                    *address)
{
   mongoc_server_description_t *server_iter = description->servers;

   while (server_iter) {
      if (address == server_iter->connection_address) {
         return true;
      }
      server_iter = server_iter->next;
   }
   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_has_primary --
 *
 *       If topology has a primary, return true, else return false.
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
_mongoc_topology_description_has_primary (mongoc_topology_description_t *description)
{
   mongoc_server_description_t *server_iter = description->servers;

   while (server_iter) {
      if (server_iter->type == MONGOC_SERVER_RS_PRIMARY) {
         return true;
      }
      server_iter = server_iter->next;
   }
   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_label_unknown_member --
 *
 *       Find the server description with the given @address and if its
 *       type is UNKNOWN, set its type to @type.
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
_mongoc_topology_description_label_unknown_member (mongoc_topology_description_t *description,
                                                   const char *address,
                                                   mongoc_server_description_type_t type)
{
   mongoc_server_description_t *server_iter = description->servers;

   while (server_iter) {
      if (server_iter->connection_address == address &&
          server_iter->type == MONGOC_SERVER_UNKNOWN) {
         _mongoc_server_description_set_state(server_iter, type);
         return;
      }
      server_iter = server_iter->next;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_state --
 *
 *       Change the state of this cluster and unblock things waiting
 *       on a change of topology type.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Unblocks anything waiting on this description to change states.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_topology_description_set_state (mongoc_topology_description_t *description,
                                        mongoc_topology_description_type_t type)
{
   description->type = type;
   // TODO SDAM unblock waiters
}
