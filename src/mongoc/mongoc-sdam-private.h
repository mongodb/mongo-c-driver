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

#ifndef MONGOC_SDAM_PRIVATE_H
#define MONGOC_SDAM_PRIVATE_H

#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"

#define MONGOC_SDAM_MIN_HEARTBEAT_FREQUENCY_MS 60000
#define MONGOC_SDAM_SOCKET_CHECK_INTERVAL_MS 5000 // must be configurable
#define MONGOC_SDAM_HEARTBEAT_FREQUENCY_MS 60000 // change: must be configurable

typedef void (*transition_t)(mongoc_topology_description_t *topology,
                             mongoc_server_description_t   *server);

void _mongoc_sdam_remove_from_monitor                 (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_remove_and_check_primary            (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_check_if_has_primary                (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_update_unknown_with_standalone      (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_update_rs_from_primary              (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_update_rs_without_primary           (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_update_rs_with_primary_from_member  (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_set_topology_type_to_sharded        (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);
void _mongoc_sdam_transition_unknown_to_rs_no_primary (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server);

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
transition_t state_transitions[MONGOC_SERVER_DESCRIPTION_TYPES][MONGOC_TOPOLOGY_DESCRIPTION_TYPES] = {
   { /* UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_SHARDED */
      NULL, /* MONGOC_TOPOLOGY_RS_NO_PRIMARY */
      _mongoc_sdam_check_if_has_primary /* MONGOC_TOPOLOGY_RS_WITH_PRIMARY */
   },
   { /* STANDALONE */
      _mongoc_sdam_update_unknown_with_standalone,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_remove_and_check_primary
   },
   { /* MONGOS */
      _mongoc_sdam_set_topology_type_to_sharded,
      NULL,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_remove_and_check_primary
   },
   { /* PRIMARY */
      _mongoc_sdam_update_rs_from_primary,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_update_rs_from_primary,
      _mongoc_sdam_update_rs_from_primary
   },
   { /* SECONDARY */
      _mongoc_sdam_transition_unknown_to_rs_no_primary,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_update_rs_without_primary,
      _mongoc_sdam_update_rs_with_primary_from_member
   },
   { /* ARBITER */
      _mongoc_sdam_transition_unknown_to_rs_no_primary,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_update_rs_without_primary,
      _mongoc_sdam_update_rs_with_primary_from_member
   },
   { /* RS_OTHER */
      _mongoc_sdam_transition_unknown_to_rs_no_primary,
      _mongoc_sdam_remove_from_monitor,
      _mongoc_sdam_update_rs_without_primary,
      _mongoc_sdam_update_rs_with_primary_from_member
   },
   { /* RS_GHOST */
      NULL,
      _mongoc_sdam_remove_from_monitor,
      NULL,
      _mongoc_sdam_check_if_has_primary
   }
};

#endif
