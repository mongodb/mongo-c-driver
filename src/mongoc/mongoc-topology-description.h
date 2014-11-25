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

#ifndef MONGOC_TOPOLOGY_DESCRIPTION_H
#define MONGOC_TOPOLOGY_DESCRIPTION_H

#include "mongoc-server-description.h"

#define MONGOC_TOPOLOGY_DESCRIPTION_TYPES 4 // TODO: what about Single??

typedef enum
   {
      MONGOC_CLUSTER_TYPE_SINGLE,
      MONGOC_CLUSTER_TYPE_REPLICA_SET_NO_PRIMARY,
      MONGOC_CLUSTER_TYPE_REPLICA_SET_WITH_PRIMARY,
      MONGOC_CLUSTER_TYPE_SHARDED,
      MONGOC_CLUSTER_TYPE_UNKNOWN,
   } mongoc_topology_description_type_t;

typedef struct _mongoc_topology_description_t
{
   mongoc_topology_description_type_t type;
   mongoc_server_description_t       *servers;
   char                              *set_name;
   bool                               compatible;
   char                              *compatibility_error;

   // TODO: add a version field, for locking purposes?

   /* callback functions to trip back into the cluster */
   void                             (*remove_node_from_cluster)(char*, void*);
} mongoc_topology_description_t;

void _mongoc_topology_description_init          (mongoc_topology_description_t *description);
void _mongoc_topology_description_destroy       (mongoc_topology_description_t *description);
void _mongoc_topology_description_add_server    (mongoc_topology_description_t *description,
                                                 mongoc_server_description_t   *server);
void _mongoc_topology_description_remove_server (mongoc_topology_description_t *description,
                                                 mongoc_server_description_t   *server);
bool _mongoc_topology_description_has_server    (mongoc_topology_description_t *description,
                                                 const char                    *server);
bool _mongoc_topology_description_has_primary   (mongoc_topology_description_t *description);

#endif
