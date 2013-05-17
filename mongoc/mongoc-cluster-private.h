/*
 * Copyright 2013 10gen Inc.
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


#ifndef MONGOC_CLUSTER_PRIVATE_H
#define MONGOC_CLUSTER_PRIVATE_H


#include <bson.h>

#include "mongoc-host-list.h"
#include "mongoc-stream.h"
#include "mongoc-uri.h"


BSON_BEGIN_DECLS


#define MONGOC_CLUSTER_MAX_NODES 12


typedef enum
{
   MONGOC_CLUSTER_DIRECT,
   MONGOC_CLUSTER_REPLICA_SET,
   MONGOC_CLUSTER_SHARDED_CLUSTER,
} mongoc_cluster_mode_t;


typedef struct
{
   mongoc_host_list_t  host;
   mongoc_stream_t    *stream;
   bson_bool_t         primary;
   bson_uint32_t       ping_msec;
} mongoc_cluster_node_t;


typedef struct
{
   mongoc_cluster_mode_t   mode;
   mongoc_uri_t           *uri;
   mongoc_cluster_node_t   nodes[MONGOC_CLUSTER_MAX_NODES];
   void                   *client;
} mongoc_cluster_t;


void mongoc_cluster_destroy (mongoc_cluster_t      *cluster);
void mongoc_cluster_init    (mongoc_cluster_t      *cluster,
                             const mongoc_uri_t    *uri,
                             void                  *client);
void mongoc_cluster_prepare (mongoc_cluster_t      *cluster);


BSON_END_DECLS


#endif /* MONGOC_CLUSTER_PRIVATE_H */
