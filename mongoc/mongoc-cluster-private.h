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


BSON_BEGIN_DECLS


typedef enum
{
   MONGOC_CLUSTER_DIRECT,
   MONGOC_CLUSTER_REPLICA_SET,
   MONGOC_CLUSTER_SHARDED_CLUSTER,
} mongoc_cluster_mode_t;


typedef enum
{
   MONGOC_CLUSTER_FLAGS_NONE       = 0,
   MONGOC_CLUSTER_FLAGS_NO_PRIMARY = 1 << 0,
   MONGOC_CLUSTER_FLAGS_CONNECTING = 1 << 1,
} mongoc_cluster_flags_t;


typedef struct
{
   mongoc_cluster_mode_t  mode;
   mongoc_cluster_flags_t flags;
} mongoc_cluster_t;


void mongoc_cluster_seed (mongoc_cluster_t         *cluster,
                          const mongoc_host_list_t *from,
                          mongoc_stream_t          *from_stream,
                          const bson_t             *seed_info);


BSON_END_DECLS


#endif /* MONGOC_CLUSTER_PRIVATE_H */
