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
   cluster->sec_latency_ms = 15;

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

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
   bson_uint32_t i;

   bson_return_if_fail(cluster);

   mongoc_uri_destroy(cluster->uri);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].stream) {
         mongoc_stream_destroy(cluster->nodes[i].stream);
         cluster->nodes[i].stream = NULL;
         cluster->nodes[i].stamp++;
      }
   }
}


static mongoc_cluster_node_t *
mongoc_cluster_select (mongoc_cluster_t *cluster,
                       mongoc_event_t   *events,
                       size_t            events_len,
                       bson_uint32_t     hint,
                       bson_error_t     *error)
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   const bson_t *read_prefs = NULL;
   bson_uint32_t count;
   bson_uint32_t watermark;
   bson_int32_t nearest = -1;
   bson_bool_t need_primary = FALSE;
   size_t i;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(events, NULL);
   bson_return_val_if_fail(events_len, NULL);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, NULL);

   if (cluster->mode == MONGOC_CLUSTER_DIRECT) {
      return cluster->nodes[0].stream ? &cluster->nodes[0] : NULL;
   }

   /*
    * NOTE: Pipelining Events
    *
    * When pipelining events, we only obey the first read operations read
    * prefs. However, if any event requires the primary, all events are
    * pinned to the primary.
    */

   for (i = 0; i < events_len; i++) {
      switch (events[i].type) {
      case MONGOC_OPCODE_KILL_CURSORS:
      case MONGOC_OPCODE_GET_MORE:
      case MONGOC_OPCODE_MSG:
      case MONGOC_OPCODE_REPLY:
         break;
      case MONGOC_OPCODE_QUERY:
         if (!read_prefs) {
            read_prefs = events[i].query.read_prefs;
         }
         break;
      case MONGOC_OPCODE_DELETE:
      case MONGOC_OPCODE_INSERT:
      case MONGOC_OPCODE_UPDATE:
      default:
         need_primary = TRUE;
         break;
      }
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
    * Check if we failed to locate a primary.
    */
   if (need_primary) {
      return NULL;
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

   count = 0;

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (read_prefs) {
         }
         if (IS_NEARER_THAN(nodes[i], nearest)) {
            nearest = nodes[i]->ping_msec;
         }
         count++;
      }
   }

#undef IS_NEARAR_THAN

   /*
    * Filter nodes with latency outside threshold of nearest.
    */
   if (nearest != -1) {
      watermark = nearest + cluster->sec_latency_ms;
      for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
         if (nodes[i]->ping_msec > watermark) {
            nodes[i] = NULL;
         }
      }
   }

   /*
    * Choose a cluster node within threshold at random.
    */
   count = count ? rand() % count : count;
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (!count) {
            return nodes[i];
         }
         count--;
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
   cluster->nodes[0].stamp = 0;
   bson_init(&cluster->nodes[0].tags);

   stream = mongoc_client_create_stream(cluster->client, hosts, error);
   if (!stream) {
      return FALSE;
   }

   cluster->nodes[0].stream = stream;
   cluster->nodes[0].stamp++;

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
 * @events: An array of mongoc_event_t events.
 * @events_len: Number of events in @events.
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
                     mongoc_event_t   *events,
                     size_t            events_len,
                     bson_uint32_t     hint,
                     bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   size_t i;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(events, FALSE);
   bson_return_val_if_fail(events_len, FALSE);

   while (!(node = mongoc_cluster_select(cluster, events, events_len, hint, error))) {
      if (!mongoc_cluster_reconnect(cluster, error)) {
         return FALSE;
      }
   }

   BSON_ASSERT(node->stream);

   if (events_len > 1) {
      mongoc_stream_cork(node->stream);
   }

   for (i = 0; i < events_len; i++) {
      if (!mongoc_event_write(&events[i], node->stream, error)) {
         mongoc_stream_destroy(node->stream);
         node->stream = NULL;
         node->stamp++;
         return 0;
      }
   }

   if (events_len > 1) {
      mongoc_stream_uncork(node->stream);
   }

   return node->index + 1;
}


/**
 * mongoc_cluster_try_send:
 * @cluster: A mongoc_cluster_t.
 * @events: A mongoc_event_t.
 * @events_len: Number of elements in @events.
 * @hint: A cluster node hint.
 * @error: A location of a bson_error_t or NULL.
 *
 * Attempts to send @events to the target primary or secondary based on
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
                         mongoc_event_t   *events,
                         size_t            events_len,
                         bson_uint32_t     hint,
                         bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   size_t i;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(events, FALSE);
   bson_return_val_if_fail(events_len, FALSE);

   if (!(node = mongoc_cluster_select(cluster, events, events_len, hint, error))) {
      return 0;
   }

   BSON_ASSERT(node->stream);

   if (events_len > 1) {
      mongoc_stream_cork(node->stream);
   }

   for (i = 0; i < events_len; i++) {
      if (!mongoc_event_write(&events[i], node->stream, error)) {
         mongoc_stream_destroy(node->stream);
         node->stream = NULL;
         node->stamp++;
         return 0;
      }
   }

   if (events_len > 1) {
      mongoc_stream_uncork(node->stream);
   }

   return node->index + 1;
}


/**
 * bson_cluster_try_recv:
 * @cluster: A bson_cluster_t.
 * @event: (out): A mongoc_event_t to fill.
 * @hint: The node to receive from, returned from mongoc_cluster_send().
 * @error: (out): A location for a bson_error_t or NULL.
 *
 * Tries to receive the next event from a particular node in the cluster.
 * @hint should be the value returned from a successful send via
 * mongoc_cluster_send() or mongoc_cluster_try_send().
 *
 * The caller owns the content of @event if successful and should release
 * those resources with mongoc_event_destroy().
 *
 * Returns: TRUE if an event was read, otherwise FALSE and @error is set.
 */
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

   if (!mongoc_event_read(event, node->stream, cluster->max_msg_size, error)) {
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
      node->stamp++;
      return FALSE;
   }

   return TRUE;
}


/**
 * mongoc_cluster_stamp:
 * @cluster: A mongoc_cluster_t.
 * @node: The node identifier.
 *
 * Returns the stamp of the node provided. The stamp is a monotonic counter
 * that tracks changes to a node within the cluster. As changes to the node
 * instance are made, the value is incremented. This helps cursors and other
 * connection sensitive portions fail gracefully (or reset) upon loss of
 * connection.
 *
 * Returns: A 32-bit stamp indiciating the node version.
 */
bson_uint32_t
mongoc_cluster_stamp (mongoc_cluster_t *cluster,
                      bson_uint32_t     node)
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= MONGOC_CLUSTER_MAX_NODES, 0);

   return cluster->nodes[node].stamp;
}
