/*
 * Copyright 2013 MongoDB, Inc.
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


#include "mongoc-config.h"

#include <errno.h>
#ifdef MONGOC_ENABLE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif
#include <string.h>

#include "mongoc-cluster-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-config.h"
#include "mongoc-error.h"
#include "mongoc-host-list-private.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-read-prefs-private.h"
#include "mongoc-rpc-private.h"
#ifdef MONGOC_ENABLE_SASL
#include "mongoc-sasl-private.h"
#endif
#include "mongoc-b64-private.h"
#include "mongoc-scram-private.h"
#include "mongoc-socket.h"
#include "mongoc-stream-private.h"
#include "mongoc-stream-socket.h"
#include "mongoc-thread-private.h"
#include "mongoc-trace.h"
#include "mongoc-util-private.h"
#include "mongoc-write-concern-private.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster"


#ifdef _WIN32
# define strcasecmp _stricmp
#endif

#ifndef MAX_RETRY_COUNT
#define MAX_RETRY_COUNT 3
#endif


#define MIN_WIRE_VERSION 0
#define MAX_WIRE_VERSION 3

#define CHECK_CLOSED_DURATION_MSEC 1000


#ifndef UNHEALTHY_RECONNECT_TIMEOUT_USEC
/*
 * Try reconnect every 20 seconds if we are unhealthy.
 */
#define UNHEALTHY_RECONNECT_TIMEOUT_USEC (1000L * 1000L * 20L)
#endif


#define DB_AND_CMD_FROM_COLLECTION(outstr, name) \
   do { \
      const char *dot = strchr(name, '.'); \
      if (!dot || ((dot - name) > (sizeof outstr - 6))) { \
         bson_snprintf(outstr, sizeof outstr, "admin.$cmd"); \
      } else { \
         memcpy(outstr, name, dot - name); \
         memcpy(outstr + (dot - name), ".$cmd", 6); \
      } \
   } while (0)


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_update_state --
 *
 *       Check the all peer nodes to update the cluster state.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_update_state (mongoc_cluster_t *cluster)
{
   mongoc_cluster_state_t state;
   mongoc_cluster_node_t *node;
   int up_nodes = 0;
   int down_nodes = 0;
   int i;

   ENTRY;

   BSON_ASSERT(cluster);

   for (i = 0; i < cluster->nodes_len; i++) {
      node = &cluster->nodes[i];
      if (node->stamp && !node->stream) {
         down_nodes++;
      } else if (node->stream) {
         up_nodes++;
      }
   }

   if (!up_nodes && !down_nodes) {
      state = MONGOC_CLUSTER_STATE_BORN;
   } else if (!up_nodes && down_nodes) {
      state = MONGOC_CLUSTER_STATE_DEAD;
   } else if (up_nodes && !down_nodes) {
      state = MONGOC_CLUSTER_STATE_HEALTHY;
   } else {
      BSON_ASSERT(up_nodes);
      BSON_ASSERT(down_nodes);
      state = MONGOC_CLUSTER_STATE_UNHEALTHY;
   }

   cluster->state = state;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_add_peer --
 *
 *       Adds a peer to the list of peers that should be potentially
 *       connected to as part of a replicaSet.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_add_peer (mongoc_cluster_t *cluster,
                          const char       *peer)
{
   mongoc_list_t *iter;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(peer);

   MONGOC_DEBUG("Registering potential peer: %s", peer);

   for (iter = cluster->peers; iter; iter = iter->next) {
      if (!strcmp(iter->data, peer)) {
         EXIT;
      }
   }

   cluster->peers = _mongoc_list_prepend(cluster->peers, bson_strdup(peer));

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_clear_peers --
 *
 *       Clears list of cached potential peers that we've seen in the
 *       "hosts" field of replicaSet nodes.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_clear_peers (mongoc_cluster_t *cluster)
{
   ENTRY;

   BSON_ASSERT(cluster);

   _mongoc_list_foreach(cluster->peers, (void(*)(void*,void*))bson_free, NULL);
   _mongoc_list_destroy(cluster->peers);
   cluster->peers = NULL;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_node_init --
 *
 *       Initialize a mongoc_cluster_node_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_init (mongoc_cluster_node_t *node)
{
   ENTRY;

   BSON_ASSERT(node);

   memset(node, 0, sizeof *node);

   node->index = 0;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stamp = 0;
   bson_init(&node->tags);
   node->primary = 0;
   node->needs_auth = 0;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_node_track_ping --
 *
 *       Add the ping time to the mongoc_cluster_node_t.
 *       Increment the position in the ring buffer and update the
 *       rolling average.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_track_ping (mongoc_cluster_node_t *node,
                                 int32_t           ping)
{
   int total = 0;
   int count = 0;
   int i;

   BSON_ASSERT(node);

   node->pings[node->pings_pos] = ping;
   node->pings_pos = (node->pings_pos + 1) % MONGOC_CLUSTER_PING_NUM_SAMPLES;

   for (i = 0; i < MONGOC_CLUSTER_PING_NUM_SAMPLES; i++) {
      if (node->pings[i] != -1) {
         total += node->pings[i];
         count++;
      }
   }

   node->ping_avg_msec = count ? (int)((double)total / (double)count) : -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_node_destroy --
 *
 *       Destroy allocated resources within @node.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_destroy (mongoc_cluster_node_t *node)
{
   ENTRY;

   BSON_ASSERT(node);

   if (node->stream) {
      mongoc_stream_close(node->stream);
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
   }

   if (node->tags.len) {
      bson_destroy (&node->tags);
      memset (&node->tags, 0, sizeof node->tags);
   }

   bson_free (node->replSet);
   node->replSet = NULL;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_build_basic_auth_digest --
 *
 *       Computes the Basic Authentication digest using the credentials
 *       configured for @cluster and the @nonce provided.
 *
 *       The result should be freed by the caller using bson_free() when
 *       they are finished with it.
 *
 * Returns:
 *       A newly allocated string containing the digest.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static char *
_mongoc_cluster_build_basic_auth_digest (mongoc_cluster_t *cluster,
                                         const char       *nonce)
{
   const char *username;
   const char *password;
   char *password_digest;
   char *password_md5;
   char *digest_in;
   char *ret;

   ENTRY;

   /*
    * The following generates the digest to be used for basic authentication
    * with a MongoDB server. More information on the format can be found
    * at the following location:
    *
    * http://docs.mongodb.org/meta-driver/latest/legacy/
    *   implement-authentication-in-driver/
    */

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->uri);

   username = mongoc_uri_get_username(cluster->uri);
   password = mongoc_uri_get_password(cluster->uri);
   password_digest = bson_strdup_printf("%s:mongo:%s", username, password);
   password_md5 = _mongoc_hex_md5(password_digest);
   digest_in = bson_strdup_printf("%s%s%s", nonce, username, password_md5);
   ret = _mongoc_hex_md5(digest_in);
   bson_free(digest_in);
   bson_free(password_md5);
   bson_free(password_digest);

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_disconnect_node --
 *
 *       Disconnects a cluster node and reinitializes it so it may be
 *       connected to again in the future.
 *
 *       The stream is closed and destroyed.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_disconnect_node (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node)
{
   ENTRY;

   bson_return_if_fail(node);

   if (node->stream) {
      mongoc_stream_close(node->stream);
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
   }

   node->needs_auth = cluster->requires_auth;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stamp++;
   node->primary = 0;

   bson_destroy (&node->tags);
   bson_init (&node->tags);

   _mongoc_cluster_update_state (cluster);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_init --
 *
 *       Initializes @cluster using the @uri and @client provided. The
 *       @uri is used to determine the "mode" of the cluster. Based on the
 *       uri we can determine if we are connected to a single host, a
 *       replicaSet, or a shardedCluster.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @cluster is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_init (mongoc_cluster_t   *cluster,
                      const mongoc_uri_t *uri,
                      void               *client)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *host_iter;
   uint32_t sockettimeoutms = MONGOC_DEFAULT_SOCKETTIMEOUTMS;
   uint32_t i;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   bson_return_if_fail (cluster);
   bson_return_if_fail (uri);

   memset (cluster, 0, sizeof *cluster);

   b = mongoc_uri_get_options(uri);
   hosts = mongoc_uri_get_hosts(uri);

   if (bson_iter_init_find_case (&iter, b, "replicaSet")) {
      cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
      cluster->replSet = bson_iter_dup_utf8 (&iter, NULL);
      MONGOC_DEBUG ("Client initialized in replica set mode.");
   } else if (hosts->next) {
      cluster->mode = MONGOC_CLUSTER_SHARDED_CLUSTER;
      MONGOC_DEBUG ("Client initialized in sharded cluster mode.");
   } else {
      cluster->mode = MONGOC_CLUSTER_DIRECT;
      MONGOC_DEBUG ("Client initialized in direct mode.");
   }

   if (bson_iter_init_find_case(&iter, b, "sockettimeoutms")) {
      if (!(sockettimeoutms = bson_iter_int32 (&iter))) {
         sockettimeoutms = MONGOC_DEFAULT_SOCKETTIMEOUTMS;
      }
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
   cluster->sec_latency_ms = 15;
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = (mongoc_uri_get_username (uri) ||
                             mongoc_uri_get_auth_mechanism (uri));
   cluster->sockettimeoutms = sockettimeoutms;

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

   if (cluster->mode == MONGOC_CLUSTER_DIRECT) {
      i = 1;
   } else {
      for (host_iter = hosts, i = 0; host_iter; host_iter = host_iter->next, i++) {}
   }

   cluster->nodes = bson_malloc(i * sizeof(*cluster->nodes));
   cluster->nodes_len = i;

   for (i = 0; i < cluster->nodes_len; i++) {
      _mongoc_cluster_node_init(&cluster->nodes[i]);
      cluster->nodes[i].stamp = 0;
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_avg_msec = -1;
      cluster->nodes[i].needs_auth = cluster->requires_auth;
   }

   _mongoc_array_init (&cluster->iov, sizeof (mongoc_iovec_t));

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_destroy --
 *
 *       Clean up after @cluster and destroy all active connections.
 *       All resources for @cluster are released.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_destroy (mongoc_cluster_t *cluster) /* INOUT */
{
   uint32_t i;

   ENTRY;

   bson_return_if_fail (cluster);

   mongoc_uri_destroy (cluster->uri);

   for (i = 0; i < cluster->nodes_len; i++) {
      if (cluster->nodes[i].stream) {
         _mongoc_cluster_node_destroy (&cluster->nodes [i]);
      }
   }

   bson_free (cluster->nodes);

   bson_free (cluster->replSet);
   cluster->replSet = NULL;

   _mongoc_cluster_clear_peers (cluster);

   _mongoc_array_destroy (&cluster->iov);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_select --
 *
 *       Selects a cluster node that is suitable for handling the required
 *       set of rpc messages. The read_prefs are taken into account.
 *
 *       If any operation is a write, primary will be forced.
 *
 * Returns:
 *       A mongoc_cluster_node_t if successful; otherwise NULL and
 *       @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_cluster_node_t *
_mongoc_cluster_select (mongoc_cluster_t             *cluster,
                        mongoc_rpc_t                 *rpcs,
                        size_t                        rpcs_len,
                        uint32_t                      hint,
                        const mongoc_write_concern_t *write_concern,
                        const mongoc_read_prefs_t    *read_prefs,
                        bson_error_t                 *error)
{
   mongoc_cluster_node_t **nodes;
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   int *scores;
   int max_score = 0;
   uint32_t count;
   uint32_t watermark;
   int32_t nearest = -1;
   bool need_primary;
   bool need_secondary;
   unsigned i;
   mongoc_cluster_node_t *node = NULL;

   ENTRY;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);
   bson_return_val_if_fail(hint <= cluster->nodes_len, NULL);

   nodes = bson_malloc(sizeof(*nodes) * cluster->nodes_len);
   scores = bson_malloc(sizeof(*scores) * cluster->nodes_len);

   /*
    * We can take a few short-cut's if we are not talking to a replica set.
    */
   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT: {
      node = (cluster->nodes[0].stream ? &cluster->nodes[0] : NULL);
      goto CLEANUP;
   }
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      need_primary = false;
      need_secondary = false;
      GOTO (dispatch);
   case MONGOC_CLUSTER_REPLICA_SET:
   default:
      break;
   }

   /*
    * Determine if our read preference requires communicating with PRIMARY.
    */
   if (read_prefs)
      read_mode = mongoc_read_prefs_get_mode(read_prefs);
   need_primary = (read_mode == MONGOC_READ_PRIMARY);
   need_secondary = (read_mode == MONGOC_READ_SECONDARY);

   /*
    * Check to see if any RPCs require the primary. If so, we pin all
    * of the RPCs to the primary.
    */
   for (i = 0; !need_primary && (i < rpcs_len); i++) {
      switch (rpcs[i].header.opcode) {
      case MONGOC_OPCODE_KILL_CURSORS:
      case MONGOC_OPCODE_GET_MORE:
      case MONGOC_OPCODE_MSG:
      case MONGOC_OPCODE_REPLY:
         break;
      case MONGOC_OPCODE_QUERY:
         if ((read_mode & MONGOC_READ_SECONDARY) != 0) {
            rpcs[i].query.flags |= MONGOC_QUERY_SLAVE_OK;
         } else if (!(rpcs[i].query.flags & MONGOC_QUERY_SLAVE_OK)) {
            need_primary = true;
         }
         break;
      case MONGOC_OPCODE_DELETE:
      case MONGOC_OPCODE_INSERT:
      case MONGOC_OPCODE_UPDATE:
      default:
         need_primary = true;
         break;
      }
   }

dispatch:

   /*
    * Build our list of nodes with established connections. Short circuit if
    * we require a primary and we found one.
    */
   for (i = 0; i < cluster->nodes_len; i++) {
      if (need_primary && cluster->nodes[i].primary) {
         node = &cluster->nodes[i];
         goto CLEANUP;
      } else if (need_secondary && cluster->nodes[i].primary) {
         nodes[i] = NULL;
      } else {
         nodes[i] = cluster->nodes[i].stream ? &cluster->nodes[i] : NULL;
      }
   }

   /*
    * Check if we failed to locate a primary.
    */
   if (need_primary) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                     "Requested PRIMARY node is not available.");
      goto CLEANUP;
   }

   /*
    * Apply the hint if the client knows who they would like to continue
    * communicating with.
    */
   if (hint) {
      if (!nodes[hint - 1]) {
         bson_set_error(error,
                        MONGOC_ERROR_CLIENT,
                        MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                        "Requested node (%u) is not available.",
                        hint);
      }
      node = nodes[hint - 1];
      goto CLEANUP;
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

   /*
    * TODO: This whole section is ripe for optimization. It is very much
    *       in the fast path of command dispatching.
    */

   count = 0;

   for (i = 0; i < cluster->nodes_len; i++) {
      if (nodes[i]) {
         if (read_prefs) {
            int score = _mongoc_read_prefs_score(read_prefs, nodes[i]);
            scores[i] = score;

            if (score < 0) {
               nodes[i] = NULL;
               continue;
            } else if (score > max_score) {
                max_score = score;
            }

         }
         count++;
      }
   }

   /*
    * Filter nodes with score less than highest score.
    */
   if (max_score) {
      for (i = 0; i < cluster->nodes_len; i++) {
         if (nodes[i] && (scores[i] < max_score)) {
             nodes[i] = NULL;
             count--;
         }
      }
   }

   /*
    * Get the nearest node among those which have not been filtered out
    */
#define IS_NEARER_THAN(n, msec) \
   ((msec < 0 && (n)->ping_avg_msec >= 0) || ((n)->ping_avg_msec < msec))

   for (i = 0; i < cluster->nodes_len; i++) {
      if (nodes[i]) {
         if (IS_NEARER_THAN(nodes[i], nearest)) {
            nearest = nodes[i]->ping_avg_msec;
         }
      }
   }

#undef IS_NEARER_THAN

   /*
    * Filter nodes with latency outside threshold of nearest.
    */
   if (nearest != -1) {
      watermark = nearest + cluster->sec_latency_ms;
      for (i = 0; i < cluster->nodes_len; i++) {
         if (nodes[i]) {
            if (nodes[i]->ping_avg_msec > (int32_t)watermark) {
               nodes[i] = NULL;
               count--;
            }
         }
      }
   }

   /*
    * Mark the error as unable to locate a target node.
    */
   if (!count) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "Failed to locate a suitable MongoDB node.");
      goto CLEANUP;
   }

   /*
    * Choose a cluster node within threshold at random.
    */
   count = count ? rand() % count : count;
   for (i = 0; i < cluster->nodes_len; i++) {
      if (nodes[i]) {
         if (!count) {
            node = nodes[i];
            goto CLEANUP;
         }
         count--;
      }
   }

CLEANUP:

   bson_free(nodes);
   bson_free(scores);

   RETURN(node);
}


uint32_t
_mongoc_cluster_preselect (mongoc_cluster_t             *cluster,       /* IN */
                           mongoc_opcode_t               opcode,        /* IN */
                           const mongoc_write_concern_t *write_concern, /* IN */
                           const mongoc_read_prefs_t    *read_prefs,    /* IN */
                           bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *node;
   mongoc_rpc_t rpc = {{ 0 }};
   int retry_count = 0;
   bson_error_t scoped_error;

   BSON_ASSERT (cluster);

   rpc.header.opcode = opcode;

   while (!(node = _mongoc_cluster_select (cluster, &rpc, 1, 0, write_concern,
                                           read_prefs, &scoped_error))) {
      if ((retry_count++ == MAX_RETRY_COUNT) ||
          !_mongoc_cluster_reconnect (cluster, &scoped_error)) {
         break;
      }
   }

   if (!node && error) {
      memcpy (error, &scoped_error, sizeof (scoped_error));
   }

   return node ? (node->index + 1) : 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_run_command --
 *
 *       Helper to run a command on a given mongoc_cluster_node_t.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_run_command (mongoc_cluster_t      *cluster,
                             mongoc_cluster_node_t *node,
                             const char            *db_name,
                             const bson_t          *command,
                             bson_t                *reply,
                             bson_error_t          *error)
{
   mongoc_buffer_t buffer;
   mongoc_array_t ar;
   mongoc_rpc_t rpc;
   int32_t msg_len;
   bson_t reply_local;
   char ns[MONGOC_NAMESPACE_MAX];

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);
   BSON_ASSERT(db_name);
   BSON_ASSERT(command);

   bson_snprintf(ns, sizeof ns, "%s.$cmd", db_name);

   rpc.query.msg_len = 0;
   rpc.query.request_id = ++cluster->request_id;
   rpc.query.response_to = 0;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   rpc.query.collection = ns;
   rpc.query.skip = 0;
   rpc.query.n_return = -1;
   rpc.query.query = bson_get_data(command);
   rpc.query.fields = NULL;

   _mongoc_array_init (&ar, sizeof (mongoc_iovec_t));
   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   _mongoc_rpc_gather(&rpc, &ar);
   _mongoc_rpc_swab_to_le(&rpc);

   DUMP_IOVEC (((mongoc_iovec_t *)ar.data), ((mongoc_iovec_t *)ar.data), ar.len);
   if (!mongoc_stream_writev(node->stream, ar.data, ar.len,
                             cluster->sockettimeoutms)) {
      GOTO(failure);
   }

   if (!_mongoc_buffer_append_from_stream(&buffer, node->stream, 4,
                                          cluster->sockettimeoutms, error)) {
      GOTO(failure);
   }

   BSON_ASSERT(buffer.len == 4);

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > (1024 * 1024 * 16))) {
      GOTO(invalid_reply);
   }

   if (!_mongoc_buffer_append_from_stream(&buffer, node->stream, msg_len - 4,
                                          cluster->sockettimeoutms, error)) {
      GOTO(failure);
   }

   if (!_mongoc_rpc_scatter(&rpc, buffer.data, buffer.len)) {
      GOTO(invalid_reply);
   }
   DUMP_BYTES (&buffer, buffer.data + buffer.off, buffer.len);

   _mongoc_rpc_swab_from_le(&rpc);

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      GOTO(invalid_reply);
   }

   if (reply) {
      if (!_mongoc_rpc_reply_get_first(&rpc.reply, &reply_local)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Failed to decode reply BSON document.");
         GOTO(failure);
      }
      bson_copy_to(&reply_local, reply);
      bson_destroy(&reply_local);
   }

   _mongoc_buffer_destroy(&buffer);
   _mongoc_array_destroy(&ar);

   RETURN(true);

invalid_reply:
   bson_set_error(error,
                  MONGOC_ERROR_PROTOCOL,
                  MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                  "Invalid reply from server.");

failure:
   _mongoc_buffer_destroy(&buffer);
   _mongoc_array_destroy(&ar);

   if (reply) {
      bson_init(reply);
   }

   _mongoc_cluster_disconnect_node(cluster, node);

   RETURN(false);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_ismaster --
 *
 *       Executes an isMaster command on a given mongoc_cluster_node_t.
 *
 *       node->primary will be set to true if the node is discovered to
 *       be a primary node.
 *
 * Returns:
 *       true if successful; otehrwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_ismaster (mongoc_cluster_t      *cluster,
                          mongoc_cluster_node_t *node,
                          bson_error_t          *error)
{
   int32_t v32;
   bool ret = false;
   bson_iter_t child;
   bson_iter_t iter;
   bson_t command;
   bson_t reply;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&command);
   bson_append_int32(&command, "isMaster", 8, 1);

   if (!_mongoc_cluster_run_command (cluster, node, "admin", &command, &reply,
                                     error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      GOTO (failure);
   }

   node->primary = false;

   bson_free (node->replSet);
   node->replSet = NULL;

   if (bson_iter_init_find_case (&iter, &reply, "isMaster") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      node->primary = true;
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxMessageSizeBytes")) {
      v32 = bson_iter_int32(&iter);
      if (!cluster->max_msg_size || (v32 < (int32_t)cluster->max_msg_size)) {
         cluster->max_msg_size = v32;
      }
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxBsonObjectSize")) {
      v32 = bson_iter_int32(&iter);
	  if (!cluster->max_bson_size || (v32 < (int32_t)cluster->max_bson_size)) {
         cluster->max_bson_size = v32;
      }
   }

   if (bson_iter_init_find_case (&iter, &reply, "maxWriteBatchSize")) {
      v32 = bson_iter_int32 (&iter);
      node->max_write_batch_size = v32;
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->max_wire_version = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find_case(&iter, &reply, "minWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->min_wire_version = bson_iter_int32(&iter);
   }

   if ((node->min_wire_version > MAX_WIRE_VERSION) ||
       (node->max_wire_version < MIN_WIRE_VERSION)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                      "Failed to negotiate wire version with "
                      "cluster peer. %s is [%u,%u]. I support "
                      "[%u,%u].",
                      node->host.host_and_port,
                      node->min_wire_version,
                      node->max_wire_version,
                      MIN_WIRE_VERSION,
                      MAX_WIRE_VERSION);
      GOTO (failure);
   }

   if (bson_iter_init_find (&iter, &reply, "msg") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (0 == strcasecmp ("isdbgrid", bson_iter_utf8 (&iter, NULL)))) {
      node->isdbgrid = true;
      if (cluster->mode != MONGOC_CLUSTER_SHARDED_CLUSTER) {
         MONGOC_INFO ("Unexpectedly connected to sharded cluster: %s",
                      node->host.host_and_port);
      }
   } else {
      node->isdbgrid = false;
   }

   /*
    * If we are in replicaSet mode, we need to track our potential peers for
    * further connections.
    */
   if (cluster->mode == MONGOC_CLUSTER_REPLICA_SET) {
      if (bson_iter_init_find (&iter, &reply, "hosts") &&
          bson_iter_recurse (&iter, &child)) {
         if (node->primary) {
            _mongoc_cluster_clear_peers (cluster);
         }
         while (bson_iter_next (&child) && BSON_ITER_HOLDS_UTF8 (&child)) {
            _mongoc_cluster_add_peer (cluster, bson_iter_utf8(&child, NULL));
         }
      }
      if (bson_iter_init_find(&iter, &reply, "setName") &&
          BSON_ITER_HOLDS_UTF8(&iter)) {
         node->replSet = bson_iter_dup_utf8(&iter, NULL);
      }
      if (bson_iter_init_find(&iter, &reply, "tags") &&
          BSON_ITER_HOLDS_DOCUMENT(&iter)) {
          bson_t tags;
          uint32_t len;
          const uint8_t *data;

          bson_iter_document(&iter, &len, &data);

          if (bson_init_static(&tags, data, len)) {
              bson_copy_to(&tags, &(node->tags));
          }
      }
   }

   ret = true;

failure:
   bson_destroy(&command);
   bson_destroy(&reply);

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_ping_node --
 *
 *       Ping a remote node and return the round-trip latency.
 *
 * Returns:
 *       A 32-bit integer counting the number of milliseconds to complete.
 *       -1 if there was a failure to communicate.
 *
 * Side effects:
 *       @error is set of -1 is returned.
 *
 *--------------------------------------------------------------------------
 */

static int32_t
_mongoc_cluster_ping_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   int64_t t_begin;
   int64_t t_end;
   int32_t ret;
   bool r;
   bson_t cmd;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&cmd);
   bson_append_int32(&cmd, "ping", 4, 1);

   t_begin = bson_get_monotonic_time ();
   r = _mongoc_cluster_run_command (cluster, node, "admin", &cmd, NULL, error);
   t_end = bson_get_monotonic_time ();

   bson_destroy(&cmd);

   ret = r ? (int32_t) ((t_end - t_begin) / 1000L) : -1;

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_cr --
 *
 *       Performs authentication of @node using the credentials provided
 *       when configuring the @cluster instance.
 *
 *       This is the Challenge-Response mode of authentication.
 *
 * Returns:
 *       true if authentication was successful; otherwise false and
 *       @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_cr (mongoc_cluster_t      *cluster,
                              mongoc_cluster_node_t *node,
                              bson_error_t          *error)
{
   bson_iter_t iter;
   const char *auth_source;
   bson_t command = { 0 };
   bson_t reply = { 0 };
   char *digest;
   char *nonce;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);

   if (!(auth_source = mongoc_uri_get_auth_source(cluster->uri)) ||
       (*auth_source == '\0')) {
      auth_source = "admin";
   }

   /*
    * To authenticate a node using basic authentication, we need to first
    * get the nonce from the server. We use that to hash our password which
    * is sent as a reply to the server. If everything went good we get a
    * success notification back from the server.
    */

   /*
    * Execute the getnonce command to fetch the nonce used for generating
    * md5 digest of our password information.
    */
   bson_init (&command);
   bson_append_int32 (&command, "getnonce", 8, 1);
   if (!_mongoc_cluster_run_command (cluster, node, auth_source, &command,
                                     &reply, error)) {
      bson_destroy (&command);
      RETURN (false);
   }
   bson_destroy (&command);
   if (!bson_iter_init_find_case (&iter, &reply, "nonce")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_GETNONCE,
                      "Invalid reply from getnonce");
      bson_destroy (&reply);
      RETURN (false);
   }

   /*
    * Build our command to perform the authentication.
    */
   nonce = bson_iter_dup_utf8(&iter, NULL);
   digest = _mongoc_cluster_build_basic_auth_digest(cluster, nonce);
   bson_init(&command);
   bson_append_int32(&command, "authenticate", 12, 1);
   bson_append_utf8(&command, "user", 4,
                    mongoc_uri_get_username(cluster->uri), -1);
   bson_append_utf8(&command, "nonce", 5, nonce, -1);
   bson_append_utf8(&command, "key", 3, digest, -1);
   bson_destroy(&reply);
   bson_free(nonce);
   bson_free(digest);

   /*
    * Execute the authenticate command and check for {ok:1}
    */
   if (!_mongoc_cluster_run_command (cluster, node, auth_source, &command,
                                     &reply, error)) {
      bson_destroy (&command);
      RETURN (false);
   }

   bson_destroy (&command);

   if (!bson_iter_init_find_case(&iter, &reply, "ok") ||
       !bson_iter_as_bool(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_AUTHENTICATE,
                     "Failed to authenticate credentials.");
      bson_destroy(&reply);
      RETURN(false);
   }

   bson_destroy(&reply);

   RETURN(true);
}


#ifdef MONGOC_ENABLE_SASL
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_get_canonicalized_name --
 *
 *       Query the node to get the canonicalized name. This may happen if
 *       the node has been accessed via an alias.
 *
 *       The gssapi code will use this if canonicalizeHostname is true.
 *
 *       Some underlying layers of krb might do this for us, but they can
 *       be disabled in krb.conf.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_get_canonicalized_name (mongoc_cluster_t      *cluster, /* IN */
                                        mongoc_cluster_node_t *node,    /* IN */
                                        char                  *name,    /* OUT */
                                        size_t                 namelen, /* IN */
                                        bson_error_t          *error)   /* OUT */
{
   mongoc_stream_t *stream;
   mongoc_stream_t *tmp;
   mongoc_socket_t *sock = NULL;
   char *canonicalized;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);
   BSON_ASSERT (name);

   /*
    * Find the underlying socket used in the stream chain.
    */
   for (stream = node->stream; stream;) {
      if ((tmp = mongoc_stream_get_base_stream (stream))) {
         stream = tmp;
         continue;
      }
      break;
   }

   BSON_ASSERT (stream);

   if (stream->type == MONGOC_STREAM_SOCKET) {
      sock = mongoc_stream_socket_get_socket ((mongoc_stream_socket_t *)stream);
      if (sock) {
         canonicalized = mongoc_socket_getnameinfo (sock);
         if (canonicalized) {
            bson_snprintf (name, namelen, "%s", canonicalized);
            bson_free (canonicalized);
            RETURN (true);
         }
      }
   }

   RETURN (false);
}
#endif


#ifdef MONGOC_ENABLE_SASL
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_sasl --
 *
 *       Perform authentication for a cluster node using SASL. This is
 *       only supported for GSSAPI at the moment.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_sasl (mongoc_cluster_t      *cluster,
                                mongoc_cluster_node_t *node,
                                bson_error_t          *error)
{
   uint32_t buflen = 0;
   mongoc_sasl_t sasl;
   const bson_t *options;
   bson_iter_t iter;
   bool ret = false;
   char real_name [BSON_HOST_NAME_MAX + 1];
   const char *service_name;
   const char *mechanism;
   const char *tmpstr;
   uint8_t buf[4096] = { 0 };
   bson_t cmd;
   bson_t reply;
   int conv_id = 0;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   options = mongoc_uri_get_options (cluster->uri);

   _mongoc_sasl_init (&sasl);

   if ((mechanism = mongoc_uri_get_auth_mechanism (cluster->uri))) {
      _mongoc_sasl_set_mechanism (&sasl, mechanism);
   }

   if (bson_iter_init_find_case (&iter, options, "gssapiservicename") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (service_name = bson_iter_utf8 (&iter, NULL))) {
      _mongoc_sasl_set_service_name (&sasl, service_name);
   }

   _mongoc_sasl_set_pass (&sasl, mongoc_uri_get_password (cluster->uri));
   _mongoc_sasl_set_user (&sasl, mongoc_uri_get_username (cluster->uri));

   /*
    * If the URI requested canonicalizeHostname, we need to resolve the real
    * hostname for the IP Address and pass that to the SASL layer. Some
    * underlying GSSAPI layers will do this for us, but can be disabled in
    * their config (krb.conf).
    *
    * This allows the consumer to specify canonicalizeHostname=true in the URI
    * and have us do that for them.
    *
    * See CDRIVER-323 for more information.
    */
   if (bson_iter_init_find_case (&iter, options, "canonicalizeHostname") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      if (_mongoc_cluster_get_canonicalized_name (cluster, node, real_name,
                                                  sizeof real_name, error)) {
         _mongoc_sasl_set_service_host (&sasl, real_name);
      } else {
         _mongoc_sasl_set_service_host (&sasl, node->host.host);
      }
   } else {
      _mongoc_sasl_set_service_host (&sasl, node->host.host);
   }

   for (;;) {
      if (!_mongoc_sasl_step (&sasl, buf, buflen, buf, sizeof buf, &buflen, error)) {
         goto failure;
      }

      bson_init (&cmd);

      if (sasl.step == 1) {
         BSON_APPEND_INT32 (&cmd, "saslStart", 1);
         BSON_APPEND_UTF8 (&cmd, "mechanism", mechanism ? mechanism : "GSSAPI");
         bson_append_utf8 (&cmd, "payload", 7, (const char *)buf, buflen);
         BSON_APPEND_INT32 (&cmd, "autoAuthorize", 1);
      } else {
         BSON_APPEND_INT32 (&cmd, "saslContinue", 1);
         BSON_APPEND_INT32 (&cmd, "conversationId", conv_id);
         bson_append_utf8 (&cmd, "payload", 7, (const char *)buf, buflen);
      }

      MONGOC_INFO ("SASL: authenticating \"%s\" (step %d)",
                   mongoc_uri_get_username (cluster->uri),
                   sasl.step);

      if (!_mongoc_cluster_run_command (cluster, node, "$external", &cmd, &reply, error)) {
         bson_destroy (&cmd);
         goto failure;
      }

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      if (!bson_iter_init_find (&iter, &reply, "ok") ||
          !bson_iter_as_bool (&iter) ||
          !bson_iter_init_find (&iter, &reply, "conversationId") ||
          !BSON_ITER_HOLDS_INT32 (&iter) ||
          !(conv_id = bson_iter_int32 (&iter)) ||
          !bson_iter_init_find (&iter, &reply, "payload") ||
          !BSON_ITER_HOLDS_UTF8 (&iter)) {
         MONGOC_INFO ("SASL: authentication failed for \"%s\"",
                      mongoc_uri_get_username (cluster->uri));
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Received invalid SASL reply from MongoDB server.");
         goto failure;
      }

      tmpstr = bson_iter_utf8 (&iter, &buflen);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "SASL reply from MongoDB is too large.");
         goto failure;
      }

      memcpy (buf, tmpstr, buflen);

      bson_destroy (&reply);
   }

   MONGOC_INFO ("SASL: \"%s\" authenticated",
                mongoc_uri_get_username (cluster->uri));

   ret = true;

failure:
   _mongoc_sasl_destroy (&sasl);

   return ret;
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_plain --
 *
 *       Perform SASL PLAIN authentication for @node. We do this manually
 *       instead of using the SASL module because its rather simplistic.
 *
 * Returns:
 *       true if successful; otherwise false and error is set.
 *
 * Side effects:
 *       error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_plain (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node,
                                 bson_error_t          *error)
{
   char buf[4096];
   int buflen = 0;
   bson_iter_t iter;
   const char *username;
   const char *password;
   const char *errmsg = "Unknown authentication error.";
   bson_t b = BSON_INITIALIZER;
   bson_t reply;
   size_t len;
   char *str;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   username = mongoc_uri_get_username (cluster->uri);
   if (!username) {
      username = "";
   }

   password = mongoc_uri_get_password (cluster->uri);
   if (!password) {
      password = "";
   }

   str = bson_strdup_printf ("%c%s%c%s", '\0', username, '\0', password);
   len = strlen (username) + strlen (password) + 2;
   buflen = mongoc_b64_ntop ((const uint8_t *) str, len, buf, sizeof buf);
   bson_free (str);

   if (buflen == -1) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "failed base64 encoding message");
      return false;
   }

   BSON_APPEND_INT32 (&b, "saslStart", 1);
   BSON_APPEND_UTF8 (&b, "mechanism", "PLAIN");
   bson_append_utf8 (&b, "payload", 7, (const char *)buf, buflen);
   BSON_APPEND_INT32 (&b, "autoAuthorize", 1);

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &b, &reply, error)) {
      bson_destroy (&b);
      return false;
   }

   bson_destroy (&b);

   if (!bson_iter_init_find_case (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find_case (&iter, &reply, "errmsg") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         errmsg = bson_iter_utf8 (&iter, NULL);
      }
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);
      bson_destroy (&reply);
      return false;
   }

   bson_destroy (&reply);

   return true;
}


#ifdef MONGOC_ENABLE_SSL
static bool
_mongoc_cluster_auth_node_x509 (mongoc_cluster_t      *cluster,
                                mongoc_cluster_node_t *node,
                                bson_error_t          *error)
{
   const char *username = "";
   const char *errmsg = "X509 authentication failure";
   bson_iter_t iter;
   bool ret = false;
   bson_t cmd;
   bson_t reply;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   username = mongoc_uri_get_username(cluster->uri);
   if (username) {
      MONGOC_INFO ("X509: got username (%s) from URI", username);
   } else {
      if (!cluster->client->ssl_opts.pem_file) {
         bson_set_error (error,
               MONGOC_ERROR_CLIENT,
               MONGOC_ERROR_CLIENT_AUTHENTICATE,
               "cannot determine username "
               "please either set it as part of the connection string or "
               "call mongoc_client_set_ssl_opts() "
               "with pem file for X-509 auth.");
         return false;
      }

      if (cluster->client->pem_subject) {
         username = cluster->client->pem_subject;
         MONGOC_INFO ("X509: got username (%s) from certificate", username);
      }
   }

   bson_init (&cmd);
   BSON_APPEND_INT32 (&cmd, "authenticate", 1);
   BSON_APPEND_UTF8 (&cmd, "mechanism", "MONGODB-X509");
   BSON_APPEND_UTF8 (&cmd, "user", username);

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &cmd, &reply,
                                     error)) {
      bson_destroy (&cmd);
      return false;
   }

   if (!bson_iter_init_find (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find (&iter, &reply, "errmsg") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         errmsg = bson_iter_utf8 (&iter, NULL);
      }
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);
      goto failure;
   }

   ret = true;

failure:

   bson_destroy (&cmd);
   bson_destroy (&reply);

   return ret;
}
#endif


#ifdef MONGOC_ENABLE_SSL
static bool
_mongoc_cluster_auth_node_scram (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node,
                                 bson_error_t          *error)
{
   uint32_t buflen = 0;
   mongoc_scram_t scram;
   bson_iter_t iter;
   bool ret = false;
   const char *tmpstr;
   const char *auth_source;
   uint8_t buf[4096] = { 0 };
   bson_t cmd;
   bson_t reply;
   int conv_id = 0;
   bson_subtype_t btype;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   if (!(auth_source = mongoc_uri_get_auth_source(cluster->uri)) ||
       (*auth_source == '\0')) {
      auth_source = "admin";
   }

   _mongoc_scram_init(&scram);

   _mongoc_scram_set_pass (&scram, mongoc_uri_get_password (cluster->uri));
   _mongoc_scram_set_user (&scram, mongoc_uri_get_username (cluster->uri));

   for (;;) {
      if (!_mongoc_scram_step (&scram, buf, buflen, buf, sizeof buf, &buflen, error)) {
         goto failure;
      }

      bson_init (&cmd);

      if (scram.step == 1) {
         BSON_APPEND_INT32 (&cmd, "saslStart", 1);
         BSON_APPEND_UTF8 (&cmd, "mechanism", "SCRAM-SHA-1");
         bson_append_binary (&cmd, "payload", 7, BSON_SUBTYPE_BINARY, buf, buflen);
         BSON_APPEND_INT32 (&cmd, "autoAuthorize", 1);
      } else {
         BSON_APPEND_INT32 (&cmd, "saslContinue", 1);
         BSON_APPEND_INT32 (&cmd, "conversationId", conv_id);
         bson_append_binary (&cmd, "payload", 7, BSON_SUBTYPE_BINARY, buf, buflen);
      }

      MONGOC_INFO ("SCRAM: authenticating \"%s\" (step %d)",
                   mongoc_uri_get_username (cluster->uri),
                   scram.step);

      if (!_mongoc_cluster_run_command (cluster, node, auth_source, &cmd, &reply, error)) {
         bson_destroy (&cmd);
         goto failure;
      }

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      if (!bson_iter_init_find (&iter, &reply, "ok") ||
          !bson_iter_as_bool (&iter) ||
          !bson_iter_init_find (&iter, &reply, "conversationId") ||
          !BSON_ITER_HOLDS_INT32 (&iter) ||
          !(conv_id = bson_iter_int32 (&iter)) ||
          !bson_iter_init_find (&iter, &reply, "payload") ||
          !BSON_ITER_HOLDS_BINARY(&iter)) {
         const char *errmsg = "Received invalid SCRAM reply from MongoDB server.";

         MONGOC_INFO ("SCRAM: authentication failed for \"%s\"",
                      mongoc_uri_get_username (cluster->uri));

         if (bson_iter_init_find (&iter, &reply, "errmsg") &&
               BSON_ITER_HOLDS_UTF8 (&iter)) {
            errmsg = bson_iter_utf8 (&iter, NULL);
         }

         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "%s", errmsg);
         bson_destroy (&reply);
         goto failure;
      }

      bson_iter_binary (&iter, &btype, &buflen, (const uint8_t**)&tmpstr);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "SCRAM reply from MongoDB is too large.");
         goto failure;
      }

      memcpy (buf, tmpstr, buflen);

      bson_destroy (&reply);
   }

   MONGOC_INFO ("SCRAM: \"%s\" authenticated",
                mongoc_uri_get_username (cluster->uri));

   ret = true;

failure:
   _mongoc_scram_destroy (&scram);

   return ret;
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node --
 *
 *       Authenticate a cluster node depending on the required mechanism.
 *
 * Returns:
 *       true if authenticated. false on failure and @error is set.
 *
 * Side effects:
 *       @error is set on failure.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   bool ret = false;
   const char *mechanism;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   mechanism = mongoc_uri_get_auth_mechanism (cluster->uri);

   if (!mechanism) {
      if (node->max_wire_version < 3) {
         mechanism = "MONGODB-CR";
      } else {
         mechanism = "SCRAM-SHA-1";
      }
   }

   if (0 == strcasecmp (mechanism, "MONGODB-CR")) {
      ret = _mongoc_cluster_auth_node_cr (cluster, node, error);
#ifdef MONGOC_ENABLE_SSL
   } else if (0 == strcasecmp (mechanism, "MONGODB-X509")) {
      ret = _mongoc_cluster_auth_node_x509 (cluster, node, error);
   } else if (0 == strcasecmp (mechanism, "SCRAM-SHA-1")) {
      ret = _mongoc_cluster_auth_node_scram (cluster, node, error);
#endif
#ifdef MONGOC_ENABLE_SASL
   } else if (0 == strcasecmp (mechanism, "GSSAPI")) {
      ret = _mongoc_cluster_auth_node_sasl (cluster, node, error);
#endif
   } else if (0 == strcasecmp (mechanism, "PLAIN")) {
      ret = _mongoc_cluster_auth_node_plain (cluster, node, error);
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "The authentication mechanism \"%s\" is not supported.",
                      mechanism);
   }

   if (!ret) {
      mongoc_counter_auth_failure_inc ();
   } else {
      mongoc_counter_auth_success_inc ();
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_reconnect_direct --
 *
 *       Reconnect to our only configured node.
 *
 *       "isMaster" is run after connecting to determine PRIMARY status.
 *
 *       If the node is valid, we will also greedily authenticate the
 *       configured user if available.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_reconnect_direct (mongoc_cluster_t *cluster,
                                  bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   mongoc_cluster_node_t *node;
   mongoc_stream_t *stream;
   struct timeval timeout;

   ENTRY;

   BSON_ASSERT(cluster);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      RETURN(false);
   }

   cluster->last_reconnect = bson_get_monotonic_time();

   node = &cluster->nodes[0];

   node->index = 0;
   node->host = *hosts;
   node->needs_auth = cluster->requires_auth;
   node->primary = false;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stream = NULL;
   node->stamp++;
   bson_init(&node->tags);

   stream = _mongoc_client_create_stream (cluster->client, hosts, error);
   if (!stream) {
      RETURN (false);
   }

   node->stream = stream;
   node->stamp++;

   timeout.tv_sec = cluster->sockettimeoutms / 1000UL;
   timeout.tv_usec = (cluster->sockettimeoutms % 1000UL) * 1000UL;
   mongoc_stream_setsockopt (stream, SOL_SOCKET, SO_RCVTIMEO,
                             &timeout, sizeof timeout);
   mongoc_stream_setsockopt (stream, SOL_SOCKET, SO_SNDTIMEO,
                             &timeout, sizeof timeout);

   if (!_mongoc_cluster_ismaster (cluster, node, error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (false);
   }

   if (node->needs_auth) {
      if (!_mongoc_cluster_auth_node (cluster, node, error)) {
         _mongoc_cluster_disconnect_node (cluster, node);
         RETURN (false);
      }
      node->needs_auth = false;
   }

   _mongoc_cluster_update_state (cluster);

   RETURN (true);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_reconnect_replica_set --
 *
 *       Reconnect to replicaSet members that are unhealthy.
 *
 *       Each of them will be checked for matching replicaSet name
 *       and capabilities via an "isMaster" command.
 *
 *       The nodes will also be greedily authenticated with the
 *       configured user if available.
 *
 * Returns:
 *       true if there is an established stream that may be used,
 *       otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_reconnect_replica_set (mongoc_cluster_t *cluster,
                                       bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_cluster_node_t node;
   mongoc_cluster_node_t *saved_nodes;
   size_t saved_nodes_len;
   mongoc_host_list_t host;
   mongoc_stream_t *stream;
   mongoc_list_t *list;
   mongoc_list_t *liter;
   int32_t ping;
   const char *replSet;
   int i;
   int j;
   bool rval = false;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->mode == MONGOC_CLUSTER_REPLICA_SET);

   saved_nodes = bson_malloc0(cluster->nodes_len * sizeof(*saved_nodes));
   saved_nodes_len = cluster->nodes_len;

   MONGOC_DEBUG("Reconnecting to replica set.");

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      goto CLEANUP;
   }

   replSet = mongoc_uri_get_replica_set(cluster->uri);
   BSON_ASSERT(replSet);

   /*
    * Replica Set (Re)Connection Strategy
    * ===================================
    *
    * First we break all existing connections. This may change.
    *
    * To perform the replica set connection, we connect to each of the
    * pre-configured replicaSet nodes. (There may in fact only be one).
    *
    * TODO: We should perform this initial connection in parallel.
    *
    * Using the result of an "isMaster" on each of these nodes, we can
    * prime the cluster nodes we want to connect to.
    *
    * We then connect to all of these nodes in parallel. Once we have
    * all of the working nodes established, we can complete the process.
    *
    * We return true if any of the connections were successful, however
    * we must update the cluster health appropriately so that callers
    * that need a PRIMARY node can force reconnection.
    *
    * TODO: At some point in the future, we will need to authenticate
    *       before calling an "isMaster". But that is dependent on a
    *       few server "features" first.
    */

   cluster->last_reconnect = bson_get_monotonic_time();

   _mongoc_cluster_clear_peers (cluster);

   /*
    * Discover all the potential peers from our seeds.
    */
   for (iter = hosts; iter; iter = iter->next) {
      stream = _mongoc_client_create_stream(cluster->client, iter, error);
      if (!stream) {
         MONGOC_WARNING("Failed connection to %s", iter->host_and_port);
         continue;
      }

      _mongoc_cluster_node_init(&node);
      node.host = *iter;
      node.stream = stream;

      if (!_mongoc_cluster_ismaster (cluster, &node, error)) {
         _mongoc_cluster_node_destroy (&node);
         continue;
      }

      if (!node.replSet || !!strcmp (node.replSet, replSet)) {
         MONGOC_INFO ("%s: Got replicaSet \"%s\" expected \"%s\".",
                      iter->host_and_port,
                      node.replSet ? node.replSet : "(null)",
                      replSet);
      }

      if (node.primary) {
         _mongoc_cluster_node_destroy (&node);
         break;
      }

      _mongoc_cluster_node_destroy (&node);
   }

   list = cluster->peers;
   cluster->peers = NULL;

   /*
    * To avoid reconnecting to all of the peers, we will save the
    * functional connections (and save their ping times) so that
    * we don't waste time doing that again.
    */

   for (i = 0; i < cluster->nodes_len; i++) {
      if (cluster->nodes [i].stream) {
         saved_nodes [i].host = cluster->nodes [i].host;
         saved_nodes [i].stream = cluster->nodes [i].stream;
         cluster->nodes [i].stream = NULL;
      }
   }

   for (liter = list, i = 0; liter; liter = liter->next, i++) {}
   cluster->nodes = bson_realloc (cluster->nodes, sizeof (*cluster->nodes) * i);
   cluster->nodes_len = i;

   for (liter = list, i = 0; liter; liter = liter->next) {
      if (!_mongoc_host_list_from_string(&host, liter->data)) {
         MONGOC_WARNING("Failed to parse host and port: \"%s\"",
                        (char *)liter->data);
         continue;
      }

      stream = NULL;

      for (j = 0; j < saved_nodes_len; j++) {
         if (0 == strcmp (saved_nodes [j].host.host_and_port,
                          host.host_and_port)) {
            stream = saved_nodes [j].stream;
            saved_nodes [j].stream = NULL;
         }
      }

      if (!stream) {
         stream = _mongoc_client_create_stream (cluster->client, &host, error);

         if (!stream) {
            MONGOC_WARNING("Failed connection to %s", host.host_and_port);
            continue;
         }
      }

      _mongoc_cluster_node_init(&cluster->nodes[i]);

      cluster->nodes[i].host = host;
      cluster->nodes[i].index = i;
      cluster->nodes[i].stream = stream;
      cluster->nodes[i].needs_auth = cluster->requires_auth;

      if (!_mongoc_cluster_ismaster(cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      if (!cluster->nodes[i].replSet ||
          !!strcmp (cluster->nodes[i].replSet, replSet)) {
         MONGOC_INFO ("%s: Got replicaSet \"%s\" expected \"%s\".",
                      host.host_and_port,
                      cluster->nodes[i].replSet ? cluster->nodes[i].replSet : "(null)",
                      replSet);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            goto CLEANUP;
         }
         cluster->nodes[i].needs_auth = false;
      }

      if (-1 == (ping = _mongoc_cluster_ping_node (cluster,
                                                   &cluster->nodes[i],
                                                   error))) {
         MONGOC_INFO("%s: Lost connection during ping.",
                     host.host_and_port);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      _mongoc_cluster_node_track_ping(&cluster->nodes[i], ping);

      i++;
   }

   cluster->nodes_len = i;

   _mongoc_list_foreach(list, (void(*)(void*,void*))bson_free, NULL);
   _mongoc_list_destroy(list);

   /*
    * Cleanup all potential saved connections that were not used.
    */

   for (j = 0; j < saved_nodes_len; j++) {
      if (saved_nodes [j].stream) {
         mongoc_stream_destroy (saved_nodes [j].stream);
         saved_nodes [j].stream = NULL;
      }
   }

   if (i == 0) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                     "No acceptable peer could be found.");
      goto CLEANUP;
   }

   _mongoc_cluster_update_state (cluster);

   rval = true;

CLEANUP:

   bson_free(saved_nodes);

   RETURN(rval);
}


static bool
_mongoc_cluster_reconnect_sharded_cluster (mongoc_cluster_t *cluster,
                                           bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream;
   uint32_t i;
   int32_t ping;

   ENTRY;

   BSON_ASSERT (cluster);

   MONGOC_DEBUG ("Reconnecting to sharded cluster.");

   /*
    * Sharded Cluster (Re)Connection Strategy
    * =======================================
    *
    * First we break all existing connections. This may and probably
    * should change.
    *
    * Sharded cluster connection is pretty simple, in that we just need
    * to connect to all of the nodes that we are configured to know
    * about. The reconnect_direct case will also update things if it
    * discovers that the node it connected to was a sharded cluster.
    *
    * We need to check for "msg" field of the "isMaster" command to
    * ensure that we have connected to an "isdbgrid".
    *
    * If we can connect to all of the nodes, we are in a good state,
    * otherwise we are in an unhealthy state. If no connections were
    * established then we are in a failed state.
    */

   cluster->last_reconnect = bson_get_monotonic_time ();

   hosts = mongoc_uri_get_hosts (cluster->uri);

   /*
    * Reconnect to each of our configured hosts.
    */
   for (iter = hosts, i = 0; iter; iter = iter->next) {
      stream = _mongoc_client_create_stream (cluster->client, iter, error);

      if (!stream) {
         MONGOC_WARNING ("Failed connection to %s", iter->host_and_port);
         continue;
      }

      _mongoc_cluster_node_init (&cluster->nodes[i]);

      cluster->nodes[i].host = *iter;
      cluster->nodes[i].index = i;
      cluster->nodes[i].stream = stream;
      cluster->nodes[i].needs_auth = cluster->requires_auth;

      if (!_mongoc_cluster_ismaster (cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            RETURN (false);
         }
         cluster->nodes[i].needs_auth = false;
      }

      if (-1 == (ping = _mongoc_cluster_ping_node (cluster,
                                                   &cluster->nodes[i],
                                                   error))) {
         MONGOC_INFO ("%s: Lost connection during ping.",
                      iter->host_and_port);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      _mongoc_cluster_node_track_ping (&cluster->nodes[i], ping);

      /*
       * If this node is not a mongos, we should fail unless no
       * replicaSet was specified. If that is the case, we will assume
       * the caller meant they wanted a replicaSet and migrate to that
       * reconnection strategy.
       */
      if ((i == 0) &&
          !cluster->nodes [i].isdbgrid &&
          !mongoc_uri_get_replica_set (cluster->uri) &&
          cluster->nodes [i].replSet) {
         MONGOC_WARNING ("Found replicaSet, expected sharded cluster. "
                         "Reconnecting as replicaSet.");
         cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
         cluster->replSet = bson_strdup (cluster->nodes [i].replSet);
         return _mongoc_cluster_reconnect_replica_set (cluster, error);
      }

      i++;
   }

   if (i == 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "No acceptable peer could be found.");
      RETURN (false);
   }

   _mongoc_cluster_update_state (cluster);

   RETURN (true);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_reconnect --
 *
 *       Reconnect to the cluster nodes.
 *
 *       This is called when no nodes were available to execute an
 *       operation on.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_reconnect (mongoc_cluster_t *cluster,
                           bson_error_t     *error)
{
   bool ret;
   int i;

   ENTRY;

   bson_return_val_if_fail (cluster, false);

   /*
    * Close any lingering connections.
    *
    * TODO: We could be better about reusing connections.
    */
   for (i = 0; i < cluster->nodes_len; i++) {
       if (cluster->nodes [i].stream) {
           mongoc_stream_close (cluster->nodes [i].stream);
           mongoc_stream_destroy (cluster->nodes [i].stream);
           cluster->nodes [i].stream = NULL;
       }
   }

   _mongoc_cluster_update_state (cluster);

   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      ret = _mongoc_cluster_reconnect_direct (cluster, error);
      RETURN (ret);
   case MONGOC_CLUSTER_REPLICA_SET:
      ret = _mongoc_cluster_reconnect_replica_set (cluster, error);
      RETURN (ret);
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      ret = _mongoc_cluster_reconnect_sharded_cluster (cluster, error);
      RETURN (ret);
   default:
      break;
   }

   bson_set_error(error,
                  MONGOC_ERROR_CLIENT,
                  MONGOC_ERROR_CLIENT_NOT_READY,
                  "Unsupported cluster mode: %02x",
                  cluster->mode);

   RETURN (false);
}


bool
_mongoc_cluster_command_early (mongoc_cluster_t *cluster,
                               const char       *dbname,
                               const bson_t     *command,
                               bson_t           *reply,
                               bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   int i;

   BSON_ASSERT (cluster);
   BSON_ASSERT (cluster->state == MONGOC_CLUSTER_STATE_BORN);
   BSON_ASSERT (dbname);
   BSON_ASSERT (command);

   if (!_mongoc_cluster_reconnect (cluster, error)) {
      return false;
   }

   node = _mongoc_cluster_get_primary (cluster);

   for (i = 0; !node && i < cluster->nodes_len; i++) {
      if (cluster->nodes[i].stream) {
         node = &cluster->nodes[i];
      }
   }

   return _mongoc_cluster_run_command (cluster, node, dbname, command,
                                       reply, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_inc_egress_rpc --
 *
 *       Helper to increment the counter for a particular RPC based on
 *       it's opcode.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_mongoc_cluster_inc_egress_rpc (const mongoc_rpc_t *rpc)
{
   mongoc_counter_op_egress_total_inc();

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_egress_delete_inc();
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_egress_update_inc();
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_egress_insert_inc();
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_egress_killcursors_inc();
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_egress_getmore_inc();
      break;
   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_egress_reply_inc();
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_egress_msg_inc();
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_egress_query_inc();
      break;
   default:
      BSON_ASSERT(false);
      break;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_inc_ingress_rpc --
 *
 *       Helper to increment the counter for a particular RPC based on
 *       it's opcode.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_mongoc_cluster_inc_ingress_rpc (const mongoc_rpc_t *rpc)
{
   mongoc_counter_op_ingress_total_inc ();

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_ingress_delete_inc ();
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_ingress_update_inc ();
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_ingress_insert_inc ();
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_ingress_killcursors_inc ();
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_ingress_getmore_inc ();
      break;
   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_ingress_reply_inc ();
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_ingress_msg_inc ();
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_ingress_query_inc ();
      break;
   default:
      BSON_ASSERT (false);
      break;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_sendv --
 *
 *       Deliver an RPC to the MongoDB server.
 *
 *       If @hint is non-zero, the connection matching that hint will be
 *       used or the operation will fail. This is primarily used to force
 *       sending an RPC on the same connection as a previous RPC. This
 *       is often the case with OP_QUERY followed by OP_GETMORE.
 *
 *       @rpcs should be an array of mongoc_rpc_t that have not yet been
 *       gathered or swab'ed. The state of @rpcs is undefined after calling
 *       this function and should not be used afterwards.
 *
 *       @write_concern is optional. Providing it may cause this function
 *       to block until an operation has completed on the remote MongoDB
 *       server.
 *
 *       @read_prefs is optional and can be used to dictate which machines
 *       may be used to perform a query upon.
 *
 *       This function will continue to try to deliver an RPC until
 *       successful or the retry count has surprased.
 *
 * Returns:
 *       Zero on failure. A non-zero value is the hint of the connection
 *       that was used to communicate with a remote MongoDB server. This
 *       value may be passed as @hint in future calls to use the same
 *       connection.
 *
 *       If the result is zero, then @error will be set with information
 *       about the failure.
 *
 * Side effects:
 *       @rpcs may be mutated and should be considered invalid after calling
 *       this function.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_sendv (mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       uint32_t                 hint,
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_prefs,
                       bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   mongoc_iovec_t *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   int64_t now;
   size_t iovcnt;
   size_t i;
   bool need_gle;
   char cmdname[140];
   int retry_count = 0;

   ENTRY;

   bson_return_val_if_fail(cluster, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   /*
    * If we are in an unhealthy state, and enough time has elapsed since
    * our last reconnection, go ahead and try to perform reconnection
    * immediately.
    */
   now = bson_get_monotonic_time();
   if ((cluster->state == MONGOC_CLUSTER_STATE_DEAD) ||
       ((cluster->state == MONGOC_CLUSTER_STATE_UNHEALTHY) &&
        (cluster->last_reconnect + UNHEALTHY_RECONNECT_TIMEOUT_USEC) <= now)) {
      if (!_mongoc_cluster_reconnect(cluster, error)) {
         RETURN(false);
      }
   }

   for (;;) {
      /*
       * Try to find a node to deliver to. Since we are allowed to block in this
       * version of sendv, we try to reconnect if we cannot select a node.
       */
      while (!(node = _mongoc_cluster_select (cluster, rpcs, rpcs_len, hint,
                                              write_concern, read_prefs,
                                              error))) {
         if ((retry_count++ == MAX_RETRY_COUNT) ||
             !_mongoc_cluster_reconnect (cluster, error)) {
            RETURN (false);
         }
      }

      BSON_ASSERT(node->stream);

      if (node->last_read_msec + CHECK_CLOSED_DURATION_MSEC < now) {
         if (mongoc_stream_check_closed (node->stream)) {
            _mongoc_cluster_disconnect_node (cluster, node);
            _mongoc_cluster_reconnect (cluster, NULL);
         } else {
            node->last_read_msec = now;
            break;
         }
      } else {
         break;
      }
   }

   _mongoc_array_clear (&cluster->iov);

   /*
    * TODO: We can probably remove the need for sendv and just do send since
    * we support write concerns now. Also, we clobber our getlasterror on
    * each subsequent mutation. It's okay, since it comes out correct anyway,
    * just useless work (and technically the request_id changes).
    */

   for (i = 0; i < rpcs_len; i++) {
      _mongoc_cluster_inc_egress_rpc (&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = _mongoc_rpc_needs_gle(&rpcs[i], write_concern);
      _mongoc_rpc_gather (&rpcs[i], &cluster->iov);

	  if (rpcs[i].header.msg_len >(int32_t)cluster->max_msg_size) {
         bson_set_error(error,
                        MONGOC_ERROR_CLIENT,
                        MONGOC_ERROR_CLIENT_TOO_BIG,
                        "Attempted to send an RPC larger than the "
                        "max allowed message size. Was %u, allowed %u.",
                        rpcs[i].header.msg_len,
                        cluster->max_msg_size);
         RETURN(0);
      }

      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;
         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].insert.collection);
            break;
         case MONGOC_OPCODE_DELETE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].delete.collection);
            break;
         case MONGOC_OPCODE_UPDATE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].update.collection);
            break;
         default:
            BSON_ASSERT(false);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }
         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;
         b = _mongoc_write_concern_get_gle((void*)write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         _mongoc_rpc_gather(&gle, &cluster->iov);
         _mongoc_rpc_swab_to_le(&gle);
      }

      _mongoc_rpc_swab_to_le(&rpcs[i]);
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   BSON_ASSERT (cluster->iov.len);

   if (!mongoc_stream_writev (node->stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      char buf[128];
      char * errstr;
      errstr = bson_strerror_r(errno, buf, sizeof buf);

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      errstr);
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (0);
   }

   RETURN (node->index + 1);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_try_sendv --
 *
 *       Deliver an RPC to a remote MongoDB instance.
 *
 *       This function is similar to _mongoc_cluster_sendv() except that it
 *       will not try to reconnect until a connection has been made.
 *
 *       This is useful if you want to fire-and-forget ignoring network
 *       errors. Kill Cursors would be a candidate for this.
 *
 * Returns:
 *       0 on failure and @error is set.
 *
 *       Non-zero on success. The return value is a hint for the
 *       connection that was used to communicate with the server.
 *
 * Side effects:
 *       @rpcs will be invalid after calling this function.
 *       @error may be set if 0 is returned.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_try_sendv (mongoc_cluster_t             *cluster,
                           mongoc_rpc_t                 *rpcs,
                           size_t                        rpcs_len,
                           uint32_t                 hint,
                           const mongoc_write_concern_t *write_concern,
                           const mongoc_read_prefs_t    *read_prefs,
                           bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   mongoc_iovec_t *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   bool need_gle;
   size_t iovcnt;
   size_t i;
   char cmdname[140];

   ENTRY;

   bson_return_val_if_fail(cluster, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   if (!(node = _mongoc_cluster_select(cluster, rpcs, rpcs_len, hint,
                                       write_concern, read_prefs, error))) {
      RETURN (0);
   }

   BSON_ASSERT (node->stream);

   _mongoc_array_clear (&cluster->iov);

   for (i = 0; i < rpcs_len; i++) {
      _mongoc_cluster_inc_egress_rpc (&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = _mongoc_rpc_needs_gle (&rpcs[i], write_concern);
      _mongoc_rpc_gather (&rpcs[i], &cluster->iov);

	  if (rpcs[i].header.msg_len >(int32_t)cluster->max_msg_size) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_TOO_BIG,
                         "Attempted to send an RPC larger than the "
                         "max allowed message size. Was %u, allowed %u.",
                         rpcs[i].header.msg_len,
                         cluster->max_msg_size);
         RETURN (0);
      }

      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;

         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].insert.collection);
            break;
         case MONGOC_OPCODE_DELETE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].delete.collection);
            break;
         case MONGOC_OPCODE_UPDATE:
            gle.query.collection = rpcs[i].update.collection;
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].update.collection);
            break;
         default:
            BSON_ASSERT(false);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }

         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;

         b = _mongoc_write_concern_get_gle ((void *)write_concern);

         gle.query.query = bson_get_data (b);
         gle.query.fields = NULL;

         _mongoc_rpc_gather (&gle, &cluster->iov);
         _mongoc_rpc_swab_to_le (&gle);
      }

      _mongoc_rpc_swab_to_le (&rpcs[i]);
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   DUMP_IOVEC (iov, iov, iovcnt);

   if (!mongoc_stream_writev (node->stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      char buf[128];
      char * errstr;
      errstr = bson_strerror_r(errno, buf, sizeof buf);

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      errstr);
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (0);
   }

   RETURN(node->index + 1);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_try_recv --
 *
 *       Tries to receive the next event from the node in the cluster
 *       specified by @hint. The contents are loaded into @buffer and then
 *       scattered into the @rpc structure. @rpc is valid as long as
 *       @buffer contains the contents read into it.
 *
 *       Callers that can optimize a reuse of @buffer should do so. It
 *       can save many memory allocations.
 *
 * Returns:
 *       0 on failure and @error is set.
 *       non-zero on success where the value is the hint of the connection
 *       that was used.
 *
 * Side effects:
 *       @error if return value is zero.
 *       @rpc is set if result is non-zero.
 *       @buffer will be filled with the input data.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_try_recv (mongoc_cluster_t *cluster,
                          mongoc_rpc_t     *rpc,
                          mongoc_buffer_t  *buffer,
                          uint32_t          hint,
                          bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   int32_t msg_len;
   off_t pos;

   ENTRY;

   bson_return_val_if_fail (cluster, false);
   bson_return_val_if_fail (rpc, false);
   bson_return_val_if_fail (buffer, false);
   bson_return_val_if_fail (hint, false);
   bson_return_val_if_fail (hint <= cluster->nodes_len, false);

   /*
    * Fetch the node to communicate over.
    */
   node = &cluster->nodes[hint-1];
   if (!node->stream) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NOT_READY,
                      "Failed to receive message, lost connection to node.");
      RETURN (false);
   }

   TRACE ("Waiting for reply from \"%s\"", node->host.host_and_port);

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, 4,
                                           cluster->sockettimeoutms, error)) {
      mongoc_counter_protocol_ingress_error_inc ();
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (false);
   }

   /*
    * Read the msg length from the buffer.
    */
   memcpy (&msg_len, &buffer->data[buffer->off + pos], 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);
   if ((msg_len < 16) || (msg_len > cluster->max_msg_size)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Corrupt or malicious reply received.");
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Scatter the buffer into the rpc structure.
    */
   if (!_mongoc_rpc_scatter (rpc, &buffer->data[buffer->off + pos], msg_len)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Failed to decode reply from server.");
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   node->last_read_msec = bson_get_monotonic_time ();

   DUMP_BYTES (buffer, buffer->data + buffer->off, buffer->len);

   _mongoc_rpc_swab_from_le (rpc);

   _mongoc_cluster_inc_ingress_rpc (rpc);

   RETURN(true);
}


/**
 * _mongoc_cluster_stamp:
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
uint32_t
_mongoc_cluster_stamp (const mongoc_cluster_t *cluster,
                       uint32_t           node)
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= cluster->nodes_len, 0);

   return cluster->nodes[node].stamp;
}


/**
 * _mongoc_cluster_get_primary:
 * @cluster: A #mongoc_cluster_t.
 *
 * Fetches the node we currently believe is PRIMARY.
 *
 * Returns: A #mongoc_cluster_node_t or %NULL.
 */
mongoc_cluster_node_t *
_mongoc_cluster_get_primary (mongoc_cluster_t *cluster)
{
   uint32_t i;

   BSON_ASSERT (cluster);

   for (i = 0; i < cluster->nodes_len; i++) {
      if (cluster->nodes[i].primary) {
         return &cluster->nodes[i];
      }
   }

   return NULL;
}
