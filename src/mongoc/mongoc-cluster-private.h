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

#ifndef MONGOC_CLUSTER_PRIVATE_H
#define MONGOC_CLUSTER_PRIVATE_H

#if !defined (MONGOC_I_AM_A_DRIVER) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-array-private.h"
#include "mongoc-buffer-private.h"
#include "mongoc-config.h"
#include "mongoc-client.h"
#include "mongoc-list-private.h"
#include "mongoc-opcode.h"
#include "mongoc-read-prefs.h"
#include "mongoc-rpc-private.h"
#include "mongoc-server-description.h"
#include "mongoc-stream.h"
#include "mongoc-topology-description.h"
#include "mongoc-uri.h"
#include "mongoc-write-concern.h"


BSON_BEGIN_DECLS


#define MONGOC_CLUSTER_PING_NUM_SAMPLES 5

typedef struct _mongoc_cluster_node_t mongoc_cluster_node_t;

struct _mongoc_cluster_node_t
{
   uint32_t         id;
   mongoc_stream_t *stream;
};


typedef struct _mongoc_cluster_t
{
   uint32_t         request_id;
   uint32_t         sockettimeoutms;
   int64_t          last_reconnect;
   mongoc_uri_t    *uri;
   unsigned         requires_auth : 1;

   mongoc_client_t *client;
   int32_t          max_bson_size;
   int32_t          max_msg_size;
   uint32_t         sec_latency_ms;

   mongoc_array_t   nodes;
   int32_t          active_nodes;

   mongoc_array_t   iov;
} mongoc_cluster_t;


void                   _mongoc_cluster_remove_node (mongoc_cluster_t             *cluster,
                                                    mongoc_cluster_node_t        *node);
void                   _mongoc_cluster_add_node    (mongoc_cluster_t             *cluster,
                                                    mongoc_server_description_t  *description,
                                                    bson_error_t                 *error);
void                   _mongoc_cluster_init        (mongoc_cluster_t             *cluster,
                                                    const mongoc_uri_t           *uri,
                                                    void                         *client);
void                   _mongoc_cluster_destroy     (mongoc_cluster_t             *cluster);

BSON_END_DECLS


#endif /* MONGOC_CLUSTER_PRIVATE_H */
