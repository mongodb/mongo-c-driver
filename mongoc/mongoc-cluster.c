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


#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-log.h"


void
mongoc_cluster_init (mongoc_cluster_t   *cluster,
                     const mongoc_uri_t *uri,
                     void               *client)
{
   const mongoc_host_list_t *hosts;
   const bson_t *b;
   bson_iter_t iter;

   bson_return_if_fail(cluster);
   bson_return_if_fail(uri);

   memset(cluster, 0, sizeof *cluster);

   b = mongoc_uri_get_options(uri);
   hosts = mongoc_uri_get_hosts(uri);

   if (bson_iter_init_find_case(&iter, b, "replicaSet")) {
      cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
   } else if (hosts->next) {
      cluster->mode = MONGOC_CLUSTER_SHARDED_CLUSTER;
   } else {
      cluster->mode = MONGOC_CLUSTER_DIRECT;
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
}


void
mongoc_cluster_destroy (mongoc_cluster_t *cluster)
{
   bson_return_if_fail(cluster);

   /*
    * TODO: release resources.
    */
}


/**
 * mongoc_cluster_prepare_replica_set:
 * @cluster: A mongoc_cluster_t.
 *
 * Discover our replicaSet information from the first node that matches
 * our configured information.
 */
void
mongoc_cluster_prepare_replica_set (mongoc_cluster_t *cluster)
{
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream;
   bson_error_t error = { 0 };

   bson_return_if_fail(cluster);

   iter = mongoc_uri_get_hosts(cluster->uri);
   for (; iter; iter = iter->next) {
      stream = mongoc_client_create_stream(cluster->client, iter, &error);
      if (!stream) {
         MONGOC_WARNING("Failed to connect to %s", error.message);
         bson_error_destroy(&error);
      } else {
      }
   }
}
