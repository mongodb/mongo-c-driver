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

#define _GNU_SOURCE

#include <errno.h>
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
#include "mongoc-util-private.h"
#include "mongoc-trace.h"
#include "mongoc-write-concern-private.h"


#ifdef MONGOC_ENABLE_SASL
#include <sasl/sasl.h>
#endif


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster"


#ifndef MAX_RETRY_COUNT
#define MAX_RETRY_COUNT 3
#endif


#define MIN_WIRE_VERSION 0
#define MAX_WIRE_VERSION 2


#ifndef DEFAULT_SOCKET_TIMEOUT_MSEC
/*
 * NOTE: The default socket timeout for connections is 5 minutes. This
 *       means that if your MongoDB server dies or becomes unavailable
 *       it will take 5 minutes to detect this.
 *
 *       You can change this by providing sockettimeoutms= in your
 *       connection URI.
 */
#define DEFAULT_SOCKET_TIMEOUT_MSEC (1000L * 60L * 5L)
#endif


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
         snprintf(outstr, sizeof outstr, "admin.$cmd"); \
         outstr[sizeof outstr - 1] = '\0'; \
      } else { \
         strncpy(outstr, name, dot - name); \
         outstr[dot - name] = '\0'; \
         strncat(outstr, ".$cmd", 5); \
         outstr[sizeof outstr - 1] = '\0'; \
      } \
   } while (0)


/**
 * _mongoc_cluster_negotiate_wire_version:
 * @node: A #mongoc_cluster_t.
 *
 * Negotiate the wire-protocol version between all of our connected
 * cluster nodes.
 *
 * If we cannot negotiate a wire-version amongst all of the nodes,
 * then %FALSE is returned and the connection should be dropped.
 *
 * If we can negotiate the wire-version amongst all of the nodes,
 * then %TRUE is returned and the clusters wire-version will be
 * updated to reflect the coordinated version.
 *
 * Returns: %TRUE if we negotiated, otherwise %FALSE.
 */
static bson_int32_t
_mongoc_cluster_negotiate_wire_version (mongoc_cluster_t *cluster)
{
   mongoc_cluster_node_t *node;
   bson_int32_t min_wire_version = MIN_WIRE_VERSION;
   bson_int32_t max_wire_version = MAX_WIRE_VERSION;
   int i;

   ENTRY;

   BSON_ASSERT (cluster);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      node = &cluster->nodes[i];

      if (node->stream) {
         if ((node->min_wire_version > max_wire_version) ||
             (node->max_wire_version < min_wire_version)) {
            RETURN (FALSE);
         }

         min_wire_version = MAX (min_wire_version, node->min_wire_version);
         max_wire_version = MIN (max_wire_version, node->max_wire_version);
      }
   }

   BSON_ASSERT (min_wire_version <= max_wire_version);
   BSON_ASSERT (min_wire_version <= MAX_WIRE_VERSION);
   BSON_ASSERT (max_wire_version >= MIN_WIRE_VERSION);

   cluster->wire_version = MAX (min_wire_version, max_wire_version);

   RETURN (TRUE);
}


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

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
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

   _mongoc_list_foreach(cluster->peers, (void *)bson_free, NULL);
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
                                 bson_int32_t           ping)
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

   node->ping_avg_msec = count ? (total / (double)count) : -1;
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

   bson_destroy(&node->tags);

   bson_free(node->replSet);
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
   bson_uint32_t sockettimeoutms = DEFAULT_SOCKET_TIMEOUT_MSEC;
   bson_uint32_t i;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

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

   if (bson_iter_init_find_case(&iter, b, "sockettimeoutms")) {
      sockettimeoutms = bson_iter_int32(&iter);
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
   cluster->sec_latency_ms = 15;
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = (mongoc_uri_get_username (uri) ||
                             mongoc_uri_get_auth_mechanism (uri));
   cluster->sockettimeoutms = sockettimeoutms;
   cluster->wire_version = MAX_WIRE_VERSION;

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      _mongoc_cluster_node_init(&cluster->nodes[i]);
      cluster->nodes[i].stamp = 0;
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_avg_msec = -1;
      cluster->nodes[i].needs_auth = cluster->requires_auth;
   }

   _mongoc_array_init (&cluster->iov, sizeof(struct iovec));

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
   bson_uint32_t i;

   ENTRY;

   bson_return_if_fail (cluster);

   mongoc_uri_destroy (cluster->uri);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].stream) {
         mongoc_stream_destroy (cluster->nodes[i].stream);
         cluster->nodes[i].stream = NULL;
         cluster->nodes[i].stamp++;
      }
   }

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
                        bson_uint32_t                 hint,
                        const mongoc_write_concern_t *write_concern,
                        const mongoc_read_prefs_t    *read_prefs,
                        bson_error_t                 *error)
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   bson_uint32_t count;
   bson_uint32_t watermark;
   bson_int32_t nearest = -1;
   bson_bool_t need_primary;
   bson_bool_t need_secondary;
   int i;

   ENTRY;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, NULL);

   /*
    * We can take a few short-cut's if we are not talking to a replica set.
    */
   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      RETURN (cluster->nodes[0].stream ? &cluster->nodes[0] : NULL);
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      need_primary = FALSE;
      need_secondary = FALSE;
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
            need_primary = TRUE;
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

dispatch:

   /*
    * Build our list of nodes with established connections. Short circuit if
    * we require a primary and we found one.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (need_primary && cluster->nodes[i].primary) {
         RETURN(&cluster->nodes[i]);
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
      RETURN(NULL);
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
      RETURN(nodes[hint - 1]);
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

#define IS_NEARER_THAN(n, msec) \
   ((msec < 0 && (n)->ping_avg_msec >= 0) || ((n)->ping_avg_msec < msec))

   count = 0;

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (read_prefs) {
            int score = _mongoc_read_prefs_score(read_prefs, nodes[i]);
            if (score < 0) {
               nodes[i] = NULL;
               continue;
            }
         }
         if (IS_NEARER_THAN(nodes[i], nearest)) {
            nearest = nodes[i]->ping_avg_msec;
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
         if (nodes[i]) {
            if (nodes[i]->ping_avg_msec > watermark) {
               nodes[i] = NULL;
            }
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
            RETURN(nodes[i]);
         }
         count--;
      }
   }

   RETURN(NULL);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_run_command --
 *
 *       Helper to run a command on a given mongoc_cluster_node_t.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
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
   bson_int32_t msg_len;
   bson_t reply_local;
   char ns[MONGOC_NAMESPACE_MAX];

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);
   BSON_ASSERT(db_name);
   BSON_ASSERT(command);

   snprintf(ns, sizeof ns, "%s.$cmd", db_name);
   ns[sizeof ns - 1] = '\0';

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

   _mongoc_array_init (&ar, sizeof (struct iovec));
   _mongoc_buffer_init (&buffer, NULL, 0, NULL);

   _mongoc_rpc_gather(&rpc, &ar);
   _mongoc_rpc_swab_to_le(&rpc);

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

   _mongoc_rpc_swab_from_le(&rpc);

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      GOTO(invalid_reply);
   }

   if (reply) {
      if (!_mongoc_rpc_reply_get_first(&rpc.reply, &reply_local)) {
         GOTO(failure);
      }
      bson_copy_to(&reply_local, reply);
      bson_destroy(&reply_local);
   }

   _mongoc_buffer_destroy(&buffer);
   _mongoc_array_destroy(&ar);

   RETURN(TRUE);

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

   RETURN(FALSE);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_ismaster --
 *
 *       Executes an isMaster command on a given mongoc_cluster_node_t.
 *
 *       node->primary will be set to TRUE if the node is discovered to
 *       be a primary node.
 *
 * Returns:
 *       TRUE if successful; otehrwise FALSE and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
_mongoc_cluster_ismaster (mongoc_cluster_t      *cluster,
                         mongoc_cluster_node_t *node,
                         bson_error_t          *error)
{
   bson_int32_t v32;
   bson_bool_t ret = FALSE;
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

   node->primary = FALSE;

   bson_free (node->replSet);
   node->replSet = NULL;

   if (bson_iter_init_find_case (&iter, &reply, "isMaster") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      node->primary = TRUE;
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxMessageSizeBytes")) {
      v32 = bson_iter_int32(&iter);
      if (!cluster->max_msg_size || (v32 < cluster->max_msg_size)) {
         cluster->max_msg_size = v32;
      }
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxBsonObjectSize")) {
      v32 = bson_iter_int32(&iter);
      if (!cluster->max_bson_size || (v32 < cluster->max_bson_size)) {
         cluster->max_bson_size = v32;
      }
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->max_wire_version = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find_case(&iter, &reply, "minWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->min_wire_version = bson_iter_int32(&iter);
   }

   if (!_mongoc_cluster_negotiate_wire_version (cluster)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                      "Failed to negotiate wire version among all "
                      "cluster peers. Current wire version is %u. "
                      "%s is [%u,%u].",
                      cluster->wire_version,
                      node->host.host_and_port,
                      node->min_wire_version,
                      node->max_wire_version);
      GOTO (failure);
   }

   if (bson_iter_init_find (&iter, &reply, "msg") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (strcmp ("isdbgrid", bson_iter_utf8 (&iter, NULL)) == 0)) {
      /* TODO: is this sufficient to detect sharded clusters? */

      cluster->isdbgrid = TRUE;
      /*
       * TODO: This is actually a sharded cluster!
       */
      if (cluster->mode != MONGOC_CLUSTER_SHARDED_CLUSTER) {
         MONGOC_INFO ("Unexpectedly connected to sharded cluster: %s",
                      node->host.host_and_port);
      }
   } else {
      cluster->isdbgrid = FALSE;
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
   }

   ret = TRUE;

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

static bson_int32_t
_mongoc_cluster_ping_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   bson_int64_t t_begin;
   bson_int64_t t_end;
   bson_int32_t ret;
   bson_bool_t r;
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

   ret = r ? (bson_int32_t) ((t_end - t_begin) / 1000L) : -1;

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
 *       TRUE if authentication was successful; otherwise FALSE and
 *       @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
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

   if (!(auth_source = mongoc_uri_get_auth_source(cluster->uri))) {
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
      RETURN (FALSE);
   }
   bson_destroy (&command);
   if (!bson_iter_init_find_case (&iter, &reply, "nonce")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_GETNONCE,
                      "Invalid reply from getnonce");
      bson_destroy (&reply);
      RETURN (FALSE);
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
      RETURN (FALSE);
   }

   if (!bson_iter_init_find_case(&iter, &reply, "ok") ||
       !bson_iter_as_bool(&iter)) {
      mongoc_counter_auth_failure_inc();
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_AUTHENTICATE,
                     "Failed to authenticate credentials.");
      bson_destroy(&reply);
      RETURN(FALSE);
   }

   bson_destroy(&reply);

   mongoc_counter_auth_success_inc();

   RETURN(TRUE);
}


#ifdef MONGOC_ENABLE_SASL
static int
_sasl_get_simple (void        *context,
                  int          id,
                  const char **result,
                  unsigned    *result_len)
{
   mongoc_cluster_t *cluster = context;
   const char *user;

   switch (id) {
   case SASL_CB_AUTHNAME:
   case SASL_CB_USER:
      user = mongoc_uri_get_username (cluster->uri);
      if (user) {
         if (result) {
            *result = user;
         }
         if (result_len) {
            *result_len = strlen (user);
         }
         return SASL_OK;
      }
      break;
   default:
      MONGOC_WARNING ("Unknown SASL parameter: %d\n", id);
      break;
   }

   return SASL_FAIL;
}

#endif

#define SASL_CALLBACK_FN(_f) ((int (*) (void))(_f))


#ifdef MONGOC_ENABLE_SASL
static bson_bool_t
_mongoc_cluster_auth_node_gssapi (mongoc_cluster_t      *cluster,
                                  mongoc_cluster_node_t *node,
                                  bson_error_t          *error)
{
   const sasl_callback_t callbacks[] = {
      { SASL_CB_AUTHNAME, SASL_CALLBACK_FN (_sasl_get_simple), cluster },
      { SASL_CB_USER, SASL_CALLBACK_FN (_sasl_get_simple), cluster },
      { SASL_CB_LIST_END }
   };
   char payload[4096];
   sasl_interact_t *interact = NULL;
   const bson_t *options;
   bson_int32_t conv_id = 0;
   sasl_conn_t *conn = NULL;
   bson_iter_t iter;
   bson_bool_t ret = FALSE;
   bson_bool_t is_continue = FALSE;
   bson_bool_t done = FALSE;
   const char *mechanism = "GSSAPI";
   const char *service_name = "mongodb";
   const char *errmsg;
   const char *raw = NULL;
   unsigned payload_len = 0;
   unsigned raw_len = 0;
   bson_t cmd;
   bson_t reply;
   int status;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   sasl_client_init (NULL);

   options = mongoc_uri_get_options (cluster->uri);

   if (bson_iter_init_find_case (&iter, options, "gssapiservicename") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      service_name = bson_iter_utf8 (&iter, NULL);
   }

   status = sasl_client_new (service_name, node->host.host,
                             NULL, NULL, callbacks, 0, &conn);

   if (status != SASL_OK) {
      switch (status) {
      case SASL_NOMEM:
         errmsg = "Not enough memory for SASL client.";
         break;
      case SASL_NOMECH:
         errmsg = "No mechanism meets SASL requirement.";
         break;
      case SASL_BADPARAM:
         errmsg = "SASL programming error, please report this "
                  "error to the mongo-c-driver github issues.";
         break;
      default:
         errmsg = "Unknown SASL initialization error.";
         break;
      }

      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);

      goto failure;
   }

   status = sasl_client_start (conn, mechanism, &interact, &raw, &raw_len,
                               &mechanism);

   if (status < 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s",
                      sasl_errdetail (conn));
      goto failure;
   }

   if ((status != SASL_CONTINUE) || (0 != strcmp (mechanism, "GSSAPI"))) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "Could not negotiate SASL mechanism.");
      goto failure;
   }

   status = sasl_encode64 (raw, raw_len, payload, sizeof payload, &payload_len);

   if (status < 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s",
                      sasl_errdetail (conn));
      goto failure;
   }

again:
   bson_init (&cmd);

   if (!is_continue) {
      BSON_APPEND_INT32 (&cmd, "saslStart", 1);
      BSON_APPEND_UTF8 (&cmd, "mechanism", "GSSAPI");
      bson_append_utf8 (&cmd, "payload", 7, payload, payload_len);
      BSON_APPEND_INT32 (&cmd, "autoAuthorize", 1);
      is_continue = TRUE;
   } else {
      BSON_APPEND_INT32 (&cmd, "saslContinue", 1);
      BSON_APPEND_INT32 (&cmd, "conversationId", conv_id);
      bson_append_utf8 (&cmd, "payload", 7, payload, payload_len);
   }

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &cmd, &reply, error)) {
      goto failure;
   }

   bson_destroy (&cmd);

   if (!bson_iter_init_find_case (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find (&iter, &reply, "errmsg")) {
         errmsg = bson_iter_utf8 (&iter, NULL);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "%s", errmsg);
         bson_destroy (&reply);
         goto failure;
      }
   }

   if (bson_iter_init_find_case (&iter, &reply, "conversationId") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      conv_id = bson_iter_int32 (&iter);
   } else {
      bson_destroy (&reply);
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "Failed to retrieve converstation id from MongoDB.");
      goto failure;
   }

   done = (bson_iter_init_find (&iter, &reply, "done") &&
           bson_iter_as_bool (&iter));

   if (done) {
      goto complete;
   }

   if (bson_iter_init_find_case (&iter, &reply, "payload") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      bson_uint32_t tmplen;
      const char *tmpstr;

      tmpstr = bson_iter_utf8 (&iter, &tmplen);

      if (tmplen > sizeof payload) {
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Payload received is too large.");
         goto failure;
      }

      status = sasl_decode64 (tmpstr, tmplen, payload, sizeof payload, &payload_len);

      if (status < 0) {
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Failure decoding SASL contents.");
         goto failure;
      }

      status = sasl_client_step (conn, payload, payload_len, &interact, &raw, &raw_len);

      if (status < 0) {
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "%s", sasl_errdetail (conn));
         goto failure;
      }

      /* TODO: Deal with interaction */

      status = sasl_encode64 (raw, raw_len, payload, sizeof payload, &payload_len);

      if (status < 0) {
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Failure encoding SASL contents.");
         goto failure;
      }

      goto again;

   } else {
      bson_destroy (&reply);
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "Failed to retrieve payload from MongoDB.");
      goto failure;
   }

complete:
   ret = TRUE;

failure:
   sasl_dispose (&conn);
   sasl_client_done ();

   return ret;
}
#endif


#ifdef MONGOC_ENABLE_SASL
static bson_bool_t
_mongoc_cluster_auth_node_plain (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node,
                                 bson_error_t          *error)
{
   char buf[4096];
   unsigned buflen = 0;
   bson_iter_t iter;
   const char *username;
   const char *password;
   const char *errmsg = "Unknown authentication error.";
   bson_t b = BSON_INITIALIZER;
   bson_t reply;
   size_t len;
   char *str;
   int ret;

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
   ret = sasl_encode64 (str, len, buf, sizeof buf, &buflen);
   bson_free (str);

   if (ret != SASL_OK) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "sasl_encode64() returned %d.",
                      ret);
      return FALSE;
   }

   BSON_APPEND_INT32 (&b, "saslStart", 1);
   BSON_APPEND_UTF8 (&b, "mechanism", "PLAIN");
   BSON_APPEND_BINARY (&b, "payload", BSON_SUBTYPE_BINARY, buf, buflen);
   BSON_APPEND_INT32 (&b, "autoAuthorize", 1);

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &b, &reply, error)) {
      bson_destroy (&b);
      return FALSE;
   }

   bson_destroy (&b);

   if (!bson_iter_init_find_case (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find_case (&iter, &reply, "errmsg") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         errmsg = bson_iter_utf8 (&iter, NULL);
      }
      bson_destroy (&reply);
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);
      return FALSE;
   }

   bson_destroy (&reply);

   return TRUE;
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
 *       TRUE if authenticated. FALSE on failure and @error is set.
 *
 * Side effects:
 *       @error is set on failure.
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
_mongoc_cluster_auth_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   const char *mechanism;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   mechanism = mongoc_uri_get_auth_mechanism (cluster->uri);

   if (!mechanism) {
      mechanism = "MONGODB-CR";
   }

   if (0 == strcasecmp (mechanism, "MONGODB-CR")) {
      return _mongoc_cluster_auth_node_cr (cluster, node, error);
   } else if (0 == strcasecmp (mechanism, "MONGODB-X509")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "X509 authentication is not yet supported.");
      return FALSE;
#ifdef MONGOC_ENABLE_SASL
   } else if (0 == strcasecmp (mechanism, "GSSAPI")) {
      return _mongoc_cluster_auth_node_gssapi (cluster, node, error);
   } else if (0 == strcasecmp (mechanism, "PLAIN")) {
      return _mongoc_cluster_auth_node_plain (cluster, node, error);
#endif /* MONGOC_ENABLE_SASL */
   }

   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_AUTHENTICATE,
                   "The authentication mechanism \"%s\" is not supported.",
                   mechanism);

   return FALSE;
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
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
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
      RETURN(FALSE);
   }

   cluster->last_reconnect = bson_get_monotonic_time();

   node = &cluster->nodes[0];

   node->index = 0;
   node->host = *hosts;
   node->needs_auth = cluster->requires_auth;
   node->primary = FALSE;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stream = NULL;
   node->stamp++;
   bson_init(&node->tags);

   stream = _mongoc_client_create_stream (cluster->client, hosts, error);
   if (!stream) {
      RETURN (FALSE);
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
      RETURN (FALSE);
   }

   if (node->needs_auth) {
      if (!_mongoc_cluster_auth_node (cluster, node, error)) {
         _mongoc_cluster_disconnect_node (cluster, node);
         RETURN (FALSE);
      }
      node->needs_auth = FALSE;
   }

   _mongoc_cluster_update_state (cluster);

   RETURN (TRUE);
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
 *       TRUE if there is an established stream that may be used,
 *       otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

static bson_bool_t
_mongoc_cluster_reconnect_replica_set (mongoc_cluster_t *cluster,
                                       bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_cluster_node_t node;
   mongoc_cluster_node_t saved_nodes [MONGOC_CLUSTER_MAX_NODES];
   mongoc_host_list_t host;
   mongoc_stream_t *stream;
   mongoc_list_t *list;
   mongoc_list_t *liter;
   bson_int32_t ping;
   const char *replSet;
   int i;
   int j;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->mode == MONGOC_CLUSTER_REPLICA_SET);

   MONGOC_DEBUG("Reconnecting to replica set.");

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      RETURN(FALSE);
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
    * We return TRUE if any of the connections were successful, however
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
         MONGOC_INFO("%s: Got replicaSet \"%s\" expected \"%s\".",
                     iter->host_and_port, node.replSet, replSet);
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

   memset (saved_nodes, 0, sizeof saved_nodes);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes [i].stream) {
         saved_nodes [i].host = cluster->nodes [i].host;
         saved_nodes [i].stream = cluster->nodes [i].stream;
         cluster->nodes [i].stream = NULL;
      }
   }

   for (liter = list, i = 0;
        liter && (i < MONGOC_CLUSTER_MAX_NODES);
        liter = liter->next) {

      if (!_mongoc_host_list_from_string(&host, liter->data)) {
         MONGOC_WARNING("Failed to parse host and port: \"%s\"",
                        (char *)liter->data);
         continue;
      }

      stream = NULL;

      for (j = 0; j < MONGOC_CLUSTER_MAX_NODES; j++) {
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

      if (!_mongoc_cluster_ismaster(cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      if (!cluster->nodes[i].replSet ||
          !!strcmp (cluster->nodes[i].replSet, replSet)) {
         MONGOC_INFO ("%s: Got replicaSet \"%s\" expected \"%s\".",
                      host.host_and_port,
                      cluster->nodes[i].replSet,
                      replSet);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            RETURN (FALSE);
         }
         cluster->nodes[i].needs_auth = FALSE;
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

   _mongoc_list_foreach(list, (void *)bson_free, NULL);
   _mongoc_list_destroy(list);

   /*
    * Cleanup all potential saved connections that were not used.
    */

   for (j = 0; j < MONGOC_CLUSTER_MAX_NODES; j++) {
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
      RETURN(FALSE);
   }

   _mongoc_cluster_update_state (cluster);

   RETURN(TRUE);
}


static bson_bool_t
_mongoc_cluster_reconnect_sharded_cluster (mongoc_cluster_t *cluster,
                                           bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream;
   bson_uint32_t i;
   bson_int32_t ping;

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

      if (!_mongoc_cluster_ismaster (cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            RETURN (FALSE);
         }
         cluster->nodes[i].needs_auth = FALSE;
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

      i++;
   }

   if (i == 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "No acceptable peer could be found.");
      RETURN (FALSE);
   }

   RETURN (TRUE);
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
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
_mongoc_cluster_reconnect (mongoc_cluster_t *cluster,
                           bson_error_t     *error)
{
   bson_bool_t ret;

   ENTRY;

   bson_return_val_if_fail (cluster, FALSE);

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

   RETURN (FALSE);
}


bson_bool_t
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
      return FALSE;
   }

   node = _mongoc_cluster_get_primary (cluster);

   for (i = 0; !node && i < MONGOC_CLUSTER_MAX_NODES; i++) {
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
      BSON_ASSERT(FALSE);
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
      BSON_ASSERT (FALSE);
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
 *       @rpcs may be muted and should be considered invalid after calling
 *       this function.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bson_uint32_t
_mongoc_cluster_sendv (mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       bson_uint32_t                 hint,
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_prefs,
                       bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   bson_int64_t now;
   const bson_t *b;
   mongoc_rpc_t gle;
   struct iovec *iov;
   bson_bool_t need_gle;
   size_t iovcnt;
   size_t i;
   char cmdname[140];
   int retry_count = 0;

   ENTRY;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(rpcs, FALSE);
   bson_return_val_if_fail(rpcs_len, FALSE);

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
         RETURN(FALSE);
      }
   }

   /*
    * Try to find a node to deliver to. Since we are allowed to block in this
    * version of sendv, we try to reconnect if we cannot select a node.
    */
   while (!(node = _mongoc_cluster_select (cluster, rpcs, rpcs_len, hint,
                                           write_concern, read_prefs,
                                           error))) {
      if ((retry_count++ == MAX_RETRY_COUNT) ||
          !_mongoc_cluster_reconnect (cluster, error)) {
         RETURN (FALSE);
      }
   }

   BSON_ASSERT(node->stream);

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

      if (rpcs[i].header.msg_len > cluster->max_msg_size) {
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
            BSON_ASSERT(FALSE);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }
         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;
         b = _mongoc_write_concern_freeze((void*)write_concern);
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
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      strerror (errno));
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

bson_uint32_t
_mongoc_cluster_try_sendv (mongoc_cluster_t             *cluster,
                           mongoc_rpc_t                 *rpcs,
                           size_t                        rpcs_len,
                           bson_uint32_t                 hint,
                           const mongoc_write_concern_t *write_concern,
                           const mongoc_read_prefs_t    *read_prefs,
                           bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   struct iovec *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   bson_bool_t need_gle;
   size_t iovcnt;
   size_t i;
   char cmdname[140];

   ENTRY;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(rpcs, FALSE);
   bson_return_val_if_fail(rpcs_len, FALSE);

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

      if (rpcs[i].header.msg_len > cluster->max_msg_size) {
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
            BSON_ASSERT(FALSE);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }

         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;

         b = _mongoc_write_concern_freeze ((void *)write_concern);

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

   if (!mongoc_stream_writev (node->stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      strerror(errno));
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

bson_bool_t
_mongoc_cluster_try_recv (mongoc_cluster_t *cluster,
                          mongoc_rpc_t     *rpc,
                          mongoc_buffer_t  *buffer,
                          bson_uint32_t     hint,
                          bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   bson_int32_t msg_len;
   off_t pos;

   ENTRY;

   bson_return_val_if_fail (cluster, FALSE);
   bson_return_val_if_fail (rpc, FALSE);
   bson_return_val_if_fail (buffer, FALSE);
   bson_return_val_if_fail (hint, FALSE);
   bson_return_val_if_fail (hint <= MONGOC_CLUSTER_MAX_NODES, FALSE);

   /*
    * Fetch the node to communicate over.
    */
   node = &cluster->nodes[hint-1];
   if (!node->stream) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NOT_READY,
                      "Failed to receive message, lost connection to node.");
      RETURN (FALSE);
   }

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, 4,
                                           cluster->sockettimeoutms, error)) {
      mongoc_counter_protocol_ingress_error_inc ();
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (FALSE);
   }

   /*
    * Read the msg length from the buffer.
    */
   memcpy (&msg_len, &buffer->data[buffer->off + pos], 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);
   if ((msg_len < 16) || (msg_len > cluster->max_bson_size)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Corrupt or malicious reply received.");
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (FALSE);
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (FALSE);
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
      RETURN (FALSE);
   }

   DUMP_BYTES (buffer, buffer->data + buffer->off, buffer->len);

   _mongoc_rpc_swab_from_le (rpc);

   _mongoc_cluster_inc_ingress_rpc (rpc);

   RETURN(TRUE);
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
bson_uint32_t
_mongoc_cluster_stamp (const mongoc_cluster_t *cluster,
                       bson_uint32_t           node)
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= MONGOC_CLUSTER_MAX_NODES, 0);

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
   bson_uint32_t i;

   BSON_ASSERT (cluster);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].primary) {
         return &cluster->nodes[i];
      }
   }

   return NULL;
}
