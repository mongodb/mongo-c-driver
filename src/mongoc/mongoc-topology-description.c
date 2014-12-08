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

#include "mongoc-array-private.h"
#include "mongoc-error.h"
#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"
#include "mongoc-trace.h"

static void
_mongoc_topology_server_dtor (void *server_,
                              void *ctx_)
{
   _mongoc_server_description_destroy ((mongoc_server_description_t *)server_);
}

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

   description->type = MONGOC_TOPOLOGY_UNKNOWN;
   description->servers = mongoc_set_new(8, _mongoc_topology_server_dtor, NULL);
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
   ENTRY;

   BSON_ASSERT(description);

   // TODO TOPOLOGY_DESCRIPTION unblock waiters?

   mongoc_set_destroy(description->servers);

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
 * _mongoc_topology_description_has_primary --
 *
 *       If topology has a primary, return true, else return false.
 *
 * Returns:
 *       A pointer to the primary, or NULL.
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */
static mongoc_server_description_t *
_mongoc_topology_description_has_primary (mongoc_topology_description_t *description)
{
   mongoc_server_description_t *server_iter;
   mongoc_set_t *set = description->servers;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (server_iter->type == MONGOC_SERVER_RS_PRIMARY) {
         return server_iter;
      }
   }
   return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_description_select_within_window --
 *
 *       Given an array of suitable servers, choose one from within
 *       the latency window and return its description.
 *
 * Returns:
 *       A server description, or NULL upon failure
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

static mongoc_server_description_t *
_mongoc_topology_description_select_within_window (mongoc_array_t *suitable_servers)
{
   bson_return_val_if_fail(suitable_servers, NULL);

   // TODO SS implement properly

   // for basic functionality, always return the first element.
   if (suitable_servers->len < 1) {
      return NULL;
   }
   return _mongoc_array_index(suitable_servers, mongoc_server_description_t *, 0);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_description_suitable_servers --
 *
 *       Return an array of suitable server descriptions for this
 *       operation and read preference.
 *
 * Returns:
 *       Array of server descriptions, or NULL upon failure.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

static void
_mongoc_topology_description_suitable_servers (mongoc_array_t *set, /* OUT */
                                               mongoc_ss_optype_t optype,
                                               mongoc_topology_description_t *topology,
                                               const mongoc_read_prefs_t *read_pref)
{
   mongoc_server_description_t *server_iter;

   // TODO SS implement properly

   // for basic functionality, return the primary always.
   server_iter = _mongoc_topology_description_has_primary(topology);
   if (server_iter) {
      _mongoc_array_append_val(set, server_iter);
   }
   return;
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_description_select --
 *
 *       Return a server description of a node that is appropriate for
 *       the given read preference and operation type.
 *
 * Returns:
 *       Selected server description, or NULL upon failure.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_topology_description_select (mongoc_topology_description_t *topology,
                                     mongoc_ss_optype_t optype,
                                     const mongoc_read_prefs_t *read_pref,
                                     bson_error_t *error /* OUT */)
{
   /* timeout calculations in microseconds */
   int64_t start_time = bson_get_monotonic_time();
   int64_t now = bson_get_monotonic_time();
   int64_t timeout = (MONGOC_SS_DEFAULT_TIMEOUT_MS * 1000UL); // TODO SS use client-set timeout if available
   mongoc_array_t suitable_servers;

   ENTRY;

   if (!topology->compatible) {
      bson_set_error(error,
                     MONGOC_ERROR_SERVER_SELECTION,
                     MONGOC_ERROR_SERVER_SELECTION_TIMEOUT,
                     "Invalid topology wire version range");
      RETURN(NULL);
   }

   _mongoc_array_init(&suitable_servers, sizeof(mongoc_server_description_t *));

   // TODO SS:
   // we should also timeout when minHearbeatFrequencyMS ms have passed, per check.
   do {
      _mongoc_topology_description_suitable_servers(&suitable_servers, optype, topology, read_pref);
      if (suitable_servers.len == 0) {
         RETURN(_mongoc_topology_description_select_within_window(&suitable_servers));
      }
      // TODO SS request scan, and wait
      now = bson_get_monotonic_time();
      // TODO wait on condition variable
   } while (start_time + timeout < now);

   bson_set_error(error,
                  MONGOC_ERROR_SERVER_SELECTION,
                  MONGOC_ERROR_SERVER_SELECTION_TIMEOUT,
                  "Could not find a suitable server");
   RETURN(NULL);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_server_by_id --
 *
 *       Get the server description for @id, if that server is present
 *       in @description. Otherwise, return NULL.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_topology_description_server_by_id (mongoc_topology_description_t *description,
                                           uint32_t                       id)
{
   return mongoc_set_get(description->servers, id);
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
   mongoc_set_rm(description->servers, server->id);
   // TODO: some sort of callback to clusters?
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_topology_has_server --
 *
 *       Return a server id if @server is in the given topology,
 *       0 otherwise.
 *
 * Returns:
 *       A server id, or 0.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
uint32_t
_mongoc_topology_description_has_server (mongoc_topology_description_t *description,
                                         const char                    *address)
{
   mongoc_server_description_t *server_iter;
   mongoc_set_t *set = description->servers;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (address == server_iter->connection_address) {
         return server_iter->id;
      }
   }

   return 0;
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
   mongoc_server_description_t *server_iter;
   mongoc_set_t *set = description->servers;
   int i;

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (server_iter->connection_address == address &&
          server_iter->type == MONGOC_SERVER_UNKNOWN) {
         _mongoc_server_description_set_state(server_iter, type);
         return;
      }
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
   // TODO TOPOLOGY_DESCRIPTION unblock waiters
}



// TRANSITIONS BEGIN HERE

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_check_if_has_primary --
 *
 *       If there is a primary in topology, set topology
 *       type to RS_WITH_PRIMARY, otherwise set it to
 *       RS_NO_PRIMARY.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes the topology type.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_check_if_has_primary (mongoc_topology_description_t *topology,
                                                   mongoc_server_description_t   *server)
{
   if (_mongoc_topology_description_has_primary(topology)) {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_WITH_PRIMARY);
   }
   else {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_add_server --
 *
 *       Add the specified server to the cluster topology and return its
 *       id, or 0 if the server was not added to the topology.
 *
 * Return:
 *       The id of this new server description, or 0.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static uint32_t
_mongoc_topology_description_add_server (mongoc_topology_description_t *topology,
                                         const char                    *server)
{
   uint32_t id;
   mongoc_server_description_t *description;

   if (_mongoc_topology_description_has_server(topology, server)){
      return 0;
   }

   id = ++topology->max_server_id;

   description = bson_malloc0(sizeof *description);
   _mongoc_server_description_init(description, server, id);

   mongoc_set_add(topology->servers, id, description);
   return id;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_update_rs_from_primary --
 *
 *       First, determine that this is really the primary:
 *          -If this node isn't in the cluster, do nothing.
 *          -If the cluster's set name is null, set it to node's set name.
 *           Otherwise if the cluster's set name is different from node's,
 *           we found a rogue primary, so remove it from the cluster and
 *           check the cluster for a primary, then return.
 *          -If any of the members of cluster reports an address different
 *           from node's, node cannot be the primary.
 *       Now that we know this is the primary:
 *          -If any hosts, passives, or arbiters in node's description aren't
 *           in the cluster, add them as UNKNOWN servers and begin monitoring.
 *          -If the cluster has any servers that aren't in node's description
 *           remove them and stop monitoring.
 *       Finally, check the cluster for the new primary.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes to the cluster, possible removal of cluster nodes.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_from_primary (mongoc_topology_description_t *topology,
                                                     mongoc_server_description_t   *server)
{
   mongoc_server_description_t *server_iter;
   mongoc_set_t *set = topology->servers;
   char **member_iter;
   int i;

   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   /*
    * 'Server' can only be the primary if it has the right rs name.
    */

   if (!topology->set_name) {
      int len = strlen(server->set_name) + 1;
      topology->set_name = bson_malloc (len);
      memcpy (topology->set_name, server->set_name, len);
   }
   else if (topology->set_name != server->set_name) {
      _mongoc_topology_description_remove_server(topology, server);
      _mongoc_topology_description_check_if_has_primary(topology, server);
      return;
   }

   /*
    *'Server' is the primary! Invalidate other primaries if found.
    */

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (server_iter->id != server->id &&
          server_iter->type == MONGOC_SERVER_RS_PRIMARY) {
         _mongoc_server_description_set_state(server, MONGOC_SERVER_UNKNOWN);
         // TODO SDAM set other states to defaults?
      }
   }

   /*
    * Begin monitoring any new servers primary knows about.
    */

   member_iter = server->rs_members;
   while (member_iter) {
      if (!_mongoc_topology_description_has_server(topology, *member_iter)) {
         _mongoc_topology_description_add_server(topology, *member_iter);
      }
      member_iter++;
   }

   /*
    * Stop monitoring any servers primary doesn't know about.
    */

   for (i = 0; i < set->items_len; i++) {
      server_iter = set->items[i].item;
      if (!_mongoc_server_description_has_rs_member(server, server_iter->connection_address)) {
         _mongoc_topology_description_remove_server(topology, server_iter);
      }
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_rs_without_primary --
 *
 *       Update cluster's information when there is no primary.
 *
 * Returns:
 *       None.
 *
 * Side Effects:
 *       Alters cluster state, may remove node from cluster.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_without_primary (mongoc_topology_description_t *topology,
                                                        mongoc_server_description_t   *server)
{
   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   if (!topology->set_name) {
      topology->set_name = server->set_name;
   }
   else if (topology->set_name != server->set_name) {
      _mongoc_topology_description_remove_server(topology, server);
      return;
   }

   /* Begin monitoring any new servers that this server knows about */
   // TODO TOPOLOGY_DESCRIPTION this loop.

   /* If this server thinks there is a primary, label it POSSIBLE_PRIMARY */
   _mongoc_topology_description_label_unknown_member(topology,
                                                     server->current_primary,
                                                     MONGOC_SERVER_POSSIBLE_PRIMARY);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_rs_with_primary_from_member --
 *
 *       Update cluster's information when there is a primary, but the
 *       update is coming from another replica set member.
 *
 * Returns:
 *       None.
 *
 * Side Effects:
 *       Alters cluster state.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_with_primary_from_member (mongoc_topology_description_t *topology,
                                                                 mongoc_server_description_t   *server)
{
   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   /* set_name should never be null here */
   if (topology->set_name != server->set_name) {
      _mongoc_topology_description_remove_server(topology, server);
   }

   /* If there is no primary, label server's current_primary as the POSSIBLE_PRIMARY */
   if (!_mongoc_topology_description_has_primary(topology)) {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
      _mongoc_topology_description_label_unknown_member(topology,
                                                        server->current_primary,
                                                        MONGOC_SERVER_POSSIBLE_PRIMARY);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_topology_type_to_sharded --
 *
 *       Sets topology's type to SHARDED.
 *
 * Returns:
 *       None
 *
 * Side effects:
 *       Alter's topology's type
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_set_topology_type_to_sharded (mongoc_topology_description_t *topology,
                                                           mongoc_server_description_t   *server)
{
   _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_SHARDED);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_transition_unknown_to_rs_no_primary --
 *
 *       Encapsulates transition from cluster state UNKNOWN to
 *       RS_NO_PRIMARY. Sets the type to RS_NO_PRIMARY,
 *       then updates the replica set accordingly.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes topology state.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_transition_unknown_to_rs_no_primary (mongoc_topology_description_t *topology,
                                                                  mongoc_server_description_t   *server)
{
   _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
   _mongoc_topology_description_update_rs_without_primary(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_remove_and_check_primary --
 *
 *       Remove this server from being monitored, then check that the
 *       current topology has a primary.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Stop monitoring server.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_remove_and_check_primary (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server)
{
   _mongoc_topology_description_remove_server(topology, server);
   _mongoc_topology_description_check_if_has_primary(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_unknown_with_standalone --
 *
 *       If the cluster doesn't contain this server, do nothing.
 *       Otherwise, if the topology only has one seed, change its
 *       type to SINGLE. If the topology has multiple seeds, it does not
 *       include us, so remove this server and stop monitoring us.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes the topology type, might remove server from monitor.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_unknown_with_standalone (mongoc_topology_description_t *topology,
                                                             mongoc_server_description_t   *server)
{
   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   if (topology->servers->items_len > 1) {
      /* This cluster contains other servers, it cannot be a standalone. */
      _mongoc_topology_description_remove_server(topology, server);
   } else {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_SINGLE);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 *  This table implements the 'ToplogyType' table outlined in the Server
 *  Discovery and Monitoring spec. Each row represents a server type,
 *  and each column represents the topology type. Given a current topology
 *  type T and a newly-observed server type S, use the function at
 *  state_transions[S][T] to transition to a new state.
 *
 *  Rows should be read like so:
 *  { // server type for this row
 *     UNKNOWN,
 *     SHARDED,
 *     RS_NO_PRIMARY,
 *     RS_WITH_PRIMARY
 *  }
 *
 *--------------------------------------------------------------------------
 */

typedef void (*transition_t)(mongoc_topology_description_t *topology,
                             mongoc_server_description_t   *server);

transition_t
gSDAMTransitionTable[MONGOC_SERVER_DESCRIPTION_TYPES][MONGOC_TOPOLOGY_DESCRIPTION_TYPES] = {
   { /* UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_SHARDED */
      NULL, /* MONGOC_TOPOLOGY_RS_NO_PRIMARY */
      _mongoc_topology_description_check_if_has_primary /* MONGOC_TOPOLOGY_RS_WITH_PRIMARY */
   },
   { /* STANDALONE */
      _mongoc_topology_description_update_unknown_with_standalone,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_and_check_primary
   },
   { /* MONGOS */
      _mongoc_topology_description_set_topology_type_to_sharded,
      NULL,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_and_check_primary
   },
   { /* PRIMARY */
      _mongoc_topology_description_update_rs_from_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_from_primary,
      _mongoc_topology_description_update_rs_from_primary
   },
   { /* SECONDARY */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* ARBITER */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* RS_OTHER */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* RS_GHOST */
      NULL,
      _mongoc_topology_description_remove_server,
      NULL,
      _mongoc_topology_description_check_if_has_primary
   }
};

/*
 *--------------------------------------------------------------------------
 *
 * ismaster.
 *
 *--------------------------------------------------------------------------
 */

void _mongoc_topology_description_handle_ismaster (mongoc_topology_description_t *topology,
                                                   const bson_t *ismaster)
{
   // TODO
   // call that table
   return;
}
