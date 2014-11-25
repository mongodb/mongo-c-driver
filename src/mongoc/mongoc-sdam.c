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

#include "mongoc-sdam-private.h"

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_label_possible_primary --
 *
 *       If server's current_primary field is not NULL, then find the
 *       server in topology whose address matches, and label it as a
 *       POSSIBLE_PRIMARY.
 *
 *       NOTE: this method does NOT check if there is already a primary
 *       in the cluster.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       May change server state.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_sdam_label_possible_primary(mongoc_topology_description_t *topology,
                                    mongoc_server_description_t   *server)
{
   mongoc_server_description_t *current_server;

   if (server->current_primary) {
      current_server = topology->servers;
      while (current_server) {
         if (current_server->connection_address == server->current_primary) {
            if (current_server->type == MONGOC_SERVER_TYPE_UNKNOWN) {
               current_server->type = MONGOC_SERVER_TYPE_POSSIBLE_PRIMARY;
            }
         }
         current_server = current_server->next;
      }
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_remove_from_monitor --
 *
 *       Remove this server from being monitored.
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
_mongoc_sdam_remove_from_monitor (mongoc_topology_description_t *topology,
                                  mongoc_server_description_t   *server)
{
   _mongoc_topology_description_remove_server(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_remove_and_check_primary --
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
void
_mongoc_sdam_remove_and_check_primary (mongoc_topology_description_t *topology,
                                       mongoc_server_description_t   *server)
{
   _mongoc_sdam_remove_from_monitor(topology, server);
   _mongoc_sdam_check_if_has_primary(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_check_if_has_primary --
 *
 *       If there is a primary in topology, set topology
 *       type to REPLICA_SET_WITH_PRIMARY, otherwise set it to
 *       REPLICA_SET_NO_PRIMARY.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes the topology type.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_sdam_check_if_has_primary (mongoc_topology_description_t *topology,
                                   mongoc_server_description_t   *server)
{
   if (_mongoc_topology_description_has_primary(topology)) {
      topology->type = MONGOC_CLUSTER_TYPE_REPLICA_SET_WITH_PRIMARY;
   }
   else {
      topology->type = MONGOC_CLUSTER_TYPE_REPLICA_SET_NO_PRIMARY;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_update_unknown_with_standalone --
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
void
_mongoc_sdam_update_unknown_with_standalone (mongoc_topology_description_t *topology,
                                             mongoc_server_description_t   *server)
{
   mongoc_server_description_t *server_iter = topology->servers;

   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;
   if (server_iter->next) {
      /* this cluster contains other servers, it cannot be a standalone. */
      _mongoc_sdam_remove_from_monitor(topology, server);
   } else {
      topology->type = MONGOC_CLUSTER_TYPE_SINGLE;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_server_has_rs_member --
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
static bool
_mongoc_sdam_server_has_rs_member(mongoc_server_description_t *server,
                                  char *address)
{
   char **member_iter = server->rs_members;
   for (int i = 0; i < server->rs_members_len; member_iter++, i++) {
      if (*member_iter == address) {
         return true;
      }
   }
   return false;
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
void
_mongoc_sdam_update_rs_from_primary (mongoc_topology_description_t *topology,
                                     mongoc_server_description_t   *server)
{
   mongoc_server_description_t *current_server;
   char **current_member;

   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   /* 'Server' can only be the primary if it has the right rs name */
   if (!topology->set_name) {
      topology->set_name = server->set_name; /* TODO: how to copy? */
   }
   else if (topology->set_name != server->set_name) {
      _mongoc_sdam_remove_from_monitor(topology, server);
      _mongoc_sdam_check_if_has_primary(topology, server);
      return;
   }

   /* 'Server' is the primary! Invalidate other primaries if found */
   current_server = topology->servers;
   while (current_server) {
      if (current_server->connection_address != server->connection_address &&
          current_server->type == MONGOC_SERVER_TYPE_RS_PRIMARY ) {
         server->type = MONGOC_SERVER_TYPE_UNKNOWN;
         /* TODO: reset other states to 'defaults'? */
      }
      current_server = current_server->next;
   }

   /* Begin monitoring any new servers primary knows about */
   current_member = server->rs_members;
   for (int i = 0; i < server->rs_members_len; i++) {
      if (!_mongoc_topology_description_has_server(topology, *current_member)) {
         // TODO: begin monitoring new server
      }
      current_member++;
   }

   /* Stop monitoring any old servers primary doesn't know about */
   current_server = topology->servers;
   while (current_server) {
      if (!_mongoc_sdam_server_has_rs_member(server, current_server->connection_address)) {
         _mongoc_sdam_remove_from_monitor(topology, current_server);
      }
      current_server = current_server->next;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_add_server_to_monitor --
 *
 *       Add the specified server to the cluster topology.
 *
 * Return:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_sdam_add_server_to_monitor (mongoc_topology_description_t *topology,
                                    const char                    *server)
{
   mongoc_server_description_t description;

   if (_mongoc_topology_description_has_server(topology, server)) return;

   _mongoc_server_description_init(&description, server, -1); // TODO: determine real index

   description.next = topology->servers;
   topology->servers = &description;

   // TODO: trip into the cluster to add node.
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_update_rs_without_primary --
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
void
_mongoc_sdam_update_rs_without_primary (mongoc_topology_description_t *topology,
                                        mongoc_server_description_t   *server)
{
   char **current_address;

   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   if (!topology->set_name) {
      topology->set_name = server->set_name;
   }
   else if (topology->set_name != server->set_name) {
      _mongoc_sdam_remove_from_monitor(topology, server);
      return;
   }

   /* Begin monitoring any new servers that this server knows about */
   current_address = server->rs_members;
   for (int i = 0; i < server->rs_members_len; current_address++, i++) {
      if (!_mongoc_topology_description_has_server(topology, *current_address)) {
         // TODO: begin monitoring
      }
   }

   /* If this server thinks there is a primary, find it and label it POSSIBLE_PRIMARY */
   _mongoc_sdam_label_possible_primary(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_update_rs_with_primary_from_member --
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
void
_mongoc_sdam_update_rs_with_primary_from_member (mongoc_topology_description_t *topology,
                                                 mongoc_server_description_t   *server)
{
   if (!_mongoc_topology_description_has_server(topology, server->connection_address)) return;

   /* set_name should never be null here */
   if (topology->set_name != server->set_name) {
      _mongoc_sdam_remove_from_monitor(topology, server);
   }

   /* If there is no primary, label server's current_primary as the POSSIBLE_PRIMARY */
   if (!_mongoc_topology_description_has_primary(topology)) {
      topology->type = MONGOC_CLUSTER_TYPE_REPLICA_SET_NO_PRIMARY;
      _mongoc_sdam_label_possible_primary(topology, server);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_set_topology_type_to_sharded --
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
void
_mongoc_sdam_set_topology_type_to_sharded (mongoc_topology_description_t *topology,
                                           mongoc_server_description_t   *server)
{
   topology->type = MONGOC_CLUSTER_TYPE_SHARDED;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_sdam_transition_unknown_to_rs_no_primary --
 *
 *       Encapsulates transition from cluster state UNKNOWN to
 *       REPLICA_SET_NO_PRIMARY. Sets the type to REPLICA_SET_NO_PRIMARY,
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
void
_mongoc_sdam_transition_unknown_to_rs_no_primary (mongoc_topology_description_t *topology,
                                                  mongoc_server_description_t   *server)
{
   topology->type = MONGOC_CLUSTER_TYPE_REPLICA_SET_NO_PRIMARY;
   _mongoc_sdam_update_rs_without_primary(topology, server);
}
