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
#include "mongoc-error.h"
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
   bson_uint32_t i;
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

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_msec = -1;
      bson_init(&cluster->nodes[i].tags);
   }
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


static mongoc_cluster_node_t *
mongoc_cluster_select (mongoc_cluster_t *cluster,
                       mongoc_event_t   *event,
                       bson_uint32_t     hint,
                       bson_error_t     *error)
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   const bson_t *read_prefs = NULL;
   bson_uint32_t i;
   bson_int32_t nearest = -1;
   bson_bool_t need_primary = TRUE;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(event, NULL);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, NULL);

   if (cluster->mode == MONGOC_CLUSTER_DIRECT) {
      return cluster->nodes[0].stream ? &cluster->nodes[0] : NULL;
   }

   switch (event->type) {
   case MONGOC_OPCODE_KILL_CURSORS:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_REPLY:
      need_primary = FALSE;
      break;
   case MONGOC_OPCODE_QUERY:
      need_primary = FALSE;
      read_prefs = event->query.read_prefs;
      break;
   case MONGOC_OPCODE_DELETE:
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   default:
      need_primary = TRUE;
      break;
   }

   /*
    * Build our list of nodes with established connections. Short circuit if
    * we require a primary and we found one.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (need_primary && cluster->nodes[i].primary)
         return &cluster->nodes[i];
      nodes[i] = cluster->nodes[i].stream ? &cluster->nodes[i] : NULL;
   }

   /*
    * Apply the hint if the client knows who they would like to continue
    * communicating with.
    */
   if (hint) {
      return nodes[hint];
   }

   /*
    * Now, we start removing connections that don't match the requirements of
    * our requested event.
    *
    * - If read preferences are set, remove all non-matching.
    * - If slaveOk exists and is false, then remove secondaries.
    * - Find the nearest leftover node and remove those not within threshold.
    * - Select a leftover node at random.
    */

#define IS_NEARER_THAN(n, msec) \
   ((msec < 0 && (n)->ping_msec >= 0) || ((n)->ping_msec < msec))

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (read_prefs) {
         }
         if (need_primary) {
         }
         if (IS_NEARER_THAN(nodes[i], nearest)) {
            nearest = nodes[i]->ping_msec;
         }
      }
   }

#undef IS_NEARAR_THAN

   /*
    * TODO: Select available node at random instead of first matching.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         return nodes[i];
      }
   }

   return NULL;
}


static bson_bool_t
mongoc_cluster_reconnect_direct (mongoc_cluster_t *cluster,
                                 bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   mongoc_stream_t *stream;
   bson_iter_t iter;
   bson_t *b;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(error, FALSE);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      return FALSE;
   }

   cluster->nodes[0].index = 0;
   cluster->nodes[0].host = *hosts;
   cluster->nodes[0].primary = FALSE;
   cluster->nodes[0].ping_msec = -1;
   cluster->nodes[0].stream = NULL;
   bson_init(&cluster->nodes[0].tags);

   stream = mongoc_client_create_stream(cluster->client, hosts, error);
   if (!stream) {
      return FALSE;
   }

   cluster->nodes[0].stream = stream;

   if (!(b = mongoc_stream_ismaster(stream, error))) {
      cluster->nodes[0].stream = NULL;
      mongoc_stream_destroy(stream);
      return FALSE;
   }

   if (bson_iter_init_find_case(&iter, b, "isMaster") &&
       bson_iter_bool(&iter)) {
      cluster->nodes[0].primary = TRUE;
   }

   if (bson_iter_init_find_case(&iter, b, "maxMessageSizeBytes")) {
      cluster->max_msg_size = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find_case(&iter, b, "maxBsonObjectSize")) {
      cluster->max_bson_size = bson_iter_int32(&iter);
   }

   bson_destroy(b);

   return TRUE;
}


static bson_bool_t
mongoc_cluster_reconnect (mongoc_cluster_t *cluster,
                          bson_error_t     *error)
{
   bson_return_val_if_fail(cluster, FALSE);

   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      return mongoc_cluster_reconnect_direct(cluster, error);
   case MONGOC_CLUSTER_REPLICA_SET:
      /* TODO */
      break;
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      /* TODO */
      break;
   default:
      break;
   }

   bson_set_error(error,
                  MONGOC_ERROR_CLIENT,
                  MONGOC_ERROR_CLIENT_NOT_READY,
                  "Unsupported cluster mode: %02x",
                  cluster->mode);

   return FALSE;
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
   mongoc_cluster_node_t *node;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(event, FALSE);

   while (!(node = mongoc_cluster_select(cluster, event, hint, error))) {
      if (!mongoc_cluster_reconnect(cluster, error)) {
         return FALSE;
      }
   }

   BSON_ASSERT(node->stream);

   if (!mongoc_event_write(event, node->stream, error)) {
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
      return 0;
   }

   return node->index + 1;
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


bson_bool_t
mongoc_cluster_try_recv (mongoc_cluster_t *cluster,
                         mongoc_event_t   *event,
                         bson_uint32_t     hint,
                         bson_error_t     *error)
{
   mongoc_cluster_node_t *node;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(hint, FALSE);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, FALSE);

   node = &cluster->nodes[hint-1];
   if (!node->stream) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Failed to receive message, lost connection to node.");
      return FALSE;
   }

   if (!mongoc_event_read(event, node->stream, error)) {
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
      return FALSE;
   }

   return TRUE;
}
