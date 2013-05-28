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


/**
 * mongoc_cluster_init:
 * @cluster: A mongoc_cluster_t.
 * @uri: The uri defining the cluster.
 * @client: A mongoc_client_t.
 *
 * Initializes the cluster instance using the uri and client.
 * The uri will define the mode the cluster is in, such as replica set,
 * sharded cluster, or direct connection.
 */
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
      MONGOC_INFO("Client initialized in replica set mode.");
   } else if (hosts->next) {
      cluster->mode = MONGOC_CLUSTER_SHARDED_CLUSTER;
      MONGOC_INFO("Client initialized in sharded cluster mode.");
   } else {
      cluster->mode = MONGOC_CLUSTER_DIRECT;
      MONGOC_INFO("Client initialized in direct mode.");
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
}


/**
 * mongoc_cluster_destroy:
 * @cluster: A mongoc_cluster_t.
 *
 * Cleans up a mongoc_cluster_t structure. This should only be called by
 * the owner of the cluster structure, such as mongoc_client_t.
 */
void
mongoc_cluster_destroy (mongoc_cluster_t *cluster)
{
   bson_return_if_fail(cluster);

   /*
    * TODO: release resources.
    */
}


/**
 * mongoc_cluster_send:
 * @cluster: A mongoc_cluster_t.
 * @event: A mongoc_event_t.
 * @hint: A hint for the cluster node or 0.
 * @error: A location for a bson_error_t or NULL.
 *
 * Sends an event via the cluster. If the event read preferences allow
 * querying a particular secondary then the appropriate transport will
 * be selected.
 *
 * This function will block until the state of the cluster is healthy.
 * Such case is useful on initial connection from a client.
 *
 * If you want a function that will fail fast in the case that the cluster
 * cannot immediately service the event, use mongoc_cluster_try_send().
 *
 * You can provide @hint to reselect the same cluster node.
 *
 * Returns: Greater than 0 if successful; otherwise 0 and @error is set.
 */
bson_uint32_t
mongoc_cluster_send (mongoc_cluster_t *cluster,
                     mongoc_event_t   *event,
                     bson_uint32_t     hint,
                     bson_error_t     *error)
{
   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(event, FALSE);

   return FALSE;
}


/**
 * mongoc_cluster_try_send:
 * @cluster: A mongoc_cluster_t.
 * @event: A mongoc_event_t.
 * @hint: A cluster node hint.
 * @error: A location of a bson_error_t or NULL.
 *
 * Attempts to send @event to the target primary or secondary based on
 * read preferences. If a cluster node is not immediately serviceable
 * then -1 is returned.
 *
 * The return value is the index of the cluster node that delivered the
 * event. This can be provided as @hint to reselect the same node on
 * a suplimental event.
 *
 * Returns: Greather than 0 if successful, otherwise 0.
 */
bson_uint32_t
mongoc_cluster_try_send (mongoc_cluster_t *cluster,
                         mongoc_event_t   *event,
                         bson_uint32_t     hint,
                         bson_error_t     *error)
{
   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(event, FALSE);

   return FALSE;
}
