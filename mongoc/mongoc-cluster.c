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
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-read-prefs-private.h"
#include "mongoc-rpc-private.h"
#include "mongoc-util-private.h"
#include "mongoc-write-concern-private.h"


#ifndef MAX_RETRY_COUNT
#define MAX_RETRY_COUNT 3
#endif

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
mongoc_cluster_update_state (mongoc_cluster_t *cluster) /* IN */
{
   mongoc_cluster_state_t state;
   mongoc_cluster_node_t *node;
   int up_nodes = 0;
   int down_nodes = 0;
   int i;

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
mongoc_cluster_add_peer (mongoc_cluster_t *cluster, /* IN */
                         const char       *peer)    /* IN */
{
   mongoc_list_t *iter;

   BSON_ASSERT(cluster);
   BSON_ASSERT(peer);

   MONGOC_DEBUG("Registering potential peer: %s", peer);

   for (iter = cluster->peers; iter; iter = iter->next) {
      if (!strcmp(iter->data, peer)) {
         return;
      }
   }

   cluster->peers = mongoc_list_prepend(cluster->peers, bson_strdup(peer));
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_clear_peers --
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
mongoc_cluster_clear_peers (mongoc_cluster_t *cluster) /* IN */
{
   BSON_ASSERT(cluster);

   mongoc_list_foreach(cluster->peers, (void *)bson_free, NULL);
   mongoc_list_destroy(cluster->peers);
   cluster->peers = NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_node_init --
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
mongoc_cluster_node_init (mongoc_cluster_node_t *node) /* IN */
{
   BSON_ASSERT(node);

   memset(node, 0, sizeof *node);

   node->index = 0;
   node->ping_msec = -1;
   node->stamp = 0;
   bson_init(&node->tags);
   node->primary = 0;
   node->needs_auth = 0;
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
mongoc_cluster_node_destroy (mongoc_cluster_node_t *node) /* IN */
{
   BSON_ASSERT(node);

   if (node->stream) {
      mongoc_stream_close(node->stream);
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
   }

   bson_destroy(&node->tags);
   bson_free(node->replSet);
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
mongoc_cluster_build_basic_auth_digest (mongoc_cluster_t *cluster, /* IN */
                                        const char       *nonce)   /* IN */
{
   const char *username;
   const char *password;
   char *password_digest;
   char *password_md5;
   char *digest_in;
   char *ret;

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
   password_md5 = mongoc_hex_md5(password_digest);
   digest_in = bson_strdup_printf("%s%s%s", nonce, username, password_md5);
   ret = mongoc_hex_md5(digest_in);
   bson_free(digest_in);
   bson_free(password_md5);
   bson_free(password_digest);

   return ret;
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

static void
mongoc_cluster_disconnect_node (mongoc_cluster_t      *cluster, /* IN */
                                mongoc_cluster_node_t *node)    /* INOUT */
{
   bson_return_if_fail(node);

   mongoc_stream_close(node->stream);
   mongoc_stream_destroy(node->stream);
   node->stream = NULL;

   node->needs_auth = cluster->requires_auth;
   node->ping_msec = -1;
   node->stamp++;
   node->primary = 0;

   bson_destroy(&node->tags);
   bson_init(&node->tags);

   mongoc_cluster_update_state(cluster);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_init --
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
mongoc_cluster_init (mongoc_cluster_t   *cluster, /* OUT */
                     const mongoc_uri_t *uri,     /* IN */
                     void               *client)  /* IN */
{
   const mongoc_host_list_t *hosts;
   bson_uint32_t sockettimeoutms = DEFAULT_SOCKET_TIMEOUT_MSEC;
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

   if (bson_iter_init_find_case(&iter, b, "sockettimeoutms")) {
      sockettimeoutms = bson_iter_int32(&iter);
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
   cluster->sec_latency_ms = 15;
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = !!mongoc_uri_get_username(uri);
   cluster->sockettimeoutms = sockettimeoutms;

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      mongoc_cluster_node_init(&cluster->nodes[i]);
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_msec = -1;
      cluster->nodes[i].needs_auth = cluster->requires_auth;
   }

   mongoc_array_init(&cluster->iov, sizeof(struct iovec));
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_destroy --
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
mongoc_cluster_destroy (mongoc_cluster_t *cluster) /* INOUT */
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

   mongoc_cluster_clear_peers(cluster);

   mongoc_array_destroy(&cluster->iov);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_select --
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
mongoc_cluster_select (mongoc_cluster_t             *cluster,       /* IN */
                       mongoc_rpc_t                 *rpcs,          /* IN */
                       size_t                        rpcs_len,      /* IN */
                       bson_uint32_t                 hint,          /* IN */
                       const mongoc_write_concern_t *write_concern, /* IN */
                       const mongoc_read_prefs_t    *read_prefs,    /* IN */
                       bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   bson_uint32_t count;
   bson_uint32_t watermark;
   bson_int32_t nearest = -1;
   bson_bool_t need_primary;
   bson_bool_t need_secondary;
   size_t i;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, NULL);

   /*
    * If we are in direct mode, short cut and take the first node.
    */
   if (cluster->mode == MONGOC_CLUSTER_DIRECT) {
      return cluster->nodes[0].stream ? &cluster->nodes[0] : NULL;
   }

   /*
    * If there is a hint, we need to connect to a specific node. If we have
    * already connected and do not have a connection to that node, we need
    * to fail immediately since a reconnection may not help.
    */
   if (hint && cluster->last_reconnect && !cluster->nodes[hint - 1].stream) {
      return NULL;
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

   /*
    * Build our list of nodes with established connections. Short circuit if
    * we require a primary and we found one.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (need_primary && cluster->nodes[i].primary)
         return &cluster->nodes[i];
      else if (need_secondary && cluster->nodes[i].primary)
         nodes[i] = NULL;
      else
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

   /*
    * TODO: This whole section is ripe for optimization. It is very much
    *       in the fast path of command dispatching.
    */

#define IS_NEARER_THAN(n, msec) \
   ((msec < 0 && (n)->ping_msec >= 0) || ((n)->ping_msec < msec))

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
         if (nodes[i]) {
            if (nodes[i]->ping_msec > watermark) {
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
            return nodes[i];
         }
         count--;
      }
   }

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_run_command --
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
mongoc_cluster_run_command (mongoc_cluster_t      *cluster, /* IN */
                            mongoc_cluster_node_t *node,    /* IN */
                            const char            *db_name, /* IN */
                            const bson_t          *command, /* IN */
                            bson_t                *reply,   /* OUT */
                            bson_error_t          *error)   /* OUT */
{
   mongoc_buffer_t buffer;
   mongoc_array_t ar;
   mongoc_rpc_t rpc;
   bson_int32_t msg_len;
   bson_t reply_local;
   char ns[MONGOC_NAMESPACE_MAX];

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

   mongoc_array_init(&ar, sizeof(struct iovec));
   mongoc_buffer_init(&buffer, NULL, 0, NULL);
   mongoc_rpc_gather(&rpc, &ar);
   mongoc_rpc_swab(&rpc);

   if (!mongoc_stream_writev(node->stream, ar.data, ar.len,
                             cluster->sockettimeoutms)) {
      goto failure;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, 4,
                                         cluster->sockettimeoutms, error)) {
      goto failure;
   }

   BSON_ASSERT(buffer.len == 4);

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > (1024 * 1024 * 16))) {
      goto invalid_reply;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, msg_len - 4,
                                         cluster->sockettimeoutms, error)) {
      goto failure;
   }

   if (!mongoc_rpc_scatter(&rpc, buffer.data, buffer.len)) {
      goto invalid_reply;
   }

   mongoc_rpc_swab(&rpc);

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      goto invalid_reply;
   }

   if (reply) {
      if (!mongoc_rpc_reply_get_first(&rpc.reply, &reply_local)) {
         goto failure;
      }
      bson_copy_to(&reply_local, reply);
      bson_destroy(&reply_local);
   }

   mongoc_buffer_destroy(&buffer);
   mongoc_array_destroy(&ar);

   return TRUE;

invalid_reply:
   bson_set_error(error,
                  MONGOC_ERROR_PROTOCOL,
                  MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                  "Invalid reply from server.");

failure:
   mongoc_buffer_destroy(&buffer);
   mongoc_array_destroy(&ar);

   if (reply) {
      bson_init(reply);
   }

   mongoc_cluster_disconnect_node(cluster, node);

   return FALSE;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_ismaster --
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
mongoc_cluster_ismaster (mongoc_cluster_t      *cluster, /* IN */
                         mongoc_cluster_node_t *node,    /* INOUT */
                         bson_error_t          *error)   /* OUT */
{
   bson_int32_t v32;
   bson_bool_t ret = FALSE;
   bson_iter_t child;
   bson_iter_t iter;
   bson_t command;
   bson_t reply;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&command);
   bson_append_int32(&command, "isMaster", 8, 1);

   if (!mongoc_cluster_run_command(cluster,
                                   node,
                                   "admin",
                                   &command,
                                   &reply,
                                   error)) {
      goto failure;
   }

   node->primary = FALSE;

   bson_free(node->replSet);
   node->replSet = NULL;

   if (bson_iter_init_find_case(&iter, &reply, "isMaster") &&
       BSON_ITER_HOLDS_BOOL(&iter) &&
       bson_iter_bool(&iter)) {
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

   /*
    * If we are in replicaSet mode, we need to track our potential peers for
    * further connections.
    */
   if (cluster->mode == MONGOC_CLUSTER_REPLICA_SET) {
      if (bson_iter_init_find(&iter, &reply, "hosts") &&
          bson_iter_recurse(&iter, &child)) {
         if (node->primary) {
            mongoc_cluster_clear_peers(cluster);
         }
         while (bson_iter_next(&child) && BSON_ITER_HOLDS_UTF8(&child)) {
            mongoc_cluster_add_peer(cluster, bson_iter_utf8(&child, NULL));
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

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_ping_node --
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
mongoc_cluster_ping_node (mongoc_cluster_t      *cluster, /* IN */
                          mongoc_cluster_node_t *node,    /* IN */
                          bson_error_t          *error)   /* OUT */
{
   bson_int64_t t_begin;
   bson_int64_t t_end;
   bson_bool_t r;
   bson_t cmd;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&cmd);
   bson_append_int32(&cmd, "ping", 4, 1);

   t_begin = bson_get_monotonic_time();
   r = mongoc_cluster_run_command(cluster, node, "admin", &cmd, NULL, error);
   t_end = bson_get_monotonic_time();

   bson_destroy(&cmd);

   return r ? (bson_int32_t) ((t_end - t_begin) / 1000L) : -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_auth_node --
 *
 *       Performs authentication of @node using the credentials provided
 *       when configuring the @cluster instance.
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
mongoc_cluster_auth_node (mongoc_cluster_t      *cluster, /* IN */
                          mongoc_cluster_node_t *node,    /* INOUT */
                          bson_error_t          *error)   /* OUT */
{
   bson_iter_t iter;
   const char *auth_source;
   bson_t command = { 0 };
   bson_t reply = { 0 };
   char *digest;
   char *nonce;

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
   bson_init(&command);
   bson_append_int32(&command, "getnonce", 8, 1);
   if (!mongoc_cluster_run_command(cluster,
                                   node,
                                   auth_source,
                                   &command,
                                   &reply,
                                   error)) {
      bson_destroy(&command);
      return FALSE;
   }
   bson_destroy(&command);
   if (!bson_iter_init_find_case(&iter, &reply, "nonce")) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_GETNONCE,
                     "Invalid reply from getnonce");
      bson_destroy(&reply);
      return FALSE;
   }

   /*
    * Build our command to perform the authentication.
    */
   nonce = bson_iter_dup_utf8(&iter, NULL);
   digest = mongoc_cluster_build_basic_auth_digest(cluster, nonce);
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
   if (!mongoc_cluster_run_command(cluster,
                                   node,
                                   auth_source,
                                   &command,
                                   &reply,
                                   error)) {
      bson_destroy(&command);
      return FALSE;
   }
   if (!bson_iter_init_find_case(&iter, &reply, "ok") ||
       !bson_iter_as_bool(&iter)) {
      mongoc_counter_auth_failure_inc();
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_AUTHENTICATE,
                     "Failed to authenticate credentials.");
      bson_destroy(&reply);
      return FALSE;
   }
   bson_destroy(&reply);

   mongoc_counter_auth_success_inc();

   return TRUE;
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
mongoc_cluster_reconnect_direct (mongoc_cluster_t *cluster, /* IN */
                                 bson_error_t     *error)   /* OUT */
{
   const mongoc_host_list_t *hosts;
   mongoc_cluster_node_t *node;
   mongoc_stream_t *stream;
   struct timeval timeout;

   BSON_ASSERT(cluster);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      return FALSE;
   }

   cluster->last_reconnect = bson_get_monotonic_time();

   node = &cluster->nodes[0];

   node->index = 0;
   node->host = *hosts;
   node->needs_auth = cluster->requires_auth;
   node->primary = FALSE;
   node->ping_msec = -1;
   node->stream = NULL;
   node->stamp++;
   bson_init(&node->tags);

   stream = mongoc_client_create_stream(cluster->client, hosts, error);
   if (!stream) {
      return FALSE;
   }

   node->stream = stream;
   node->stamp++;

   timeout.tv_sec = cluster->sockettimeoutms / 1000UL;
   timeout.tv_usec = (cluster->sockettimeoutms % 1000UL) * 1000UL;
   mongoc_stream_setsockopt(stream, SOL_SOCKET, SO_RCVTIMEO,
                            &timeout, sizeof timeout);
   mongoc_stream_setsockopt(stream, SOL_SOCKET, SO_SNDTIMEO,
                            &timeout, sizeof timeout);

   if (!mongoc_cluster_ismaster(cluster, node, error)) {
      mongoc_cluster_disconnect_node(cluster, node);
      return FALSE;
   }

   if (node->needs_auth) {
      if (!mongoc_cluster_auth_node(cluster, node, error)) {
         mongoc_cluster_disconnect_node(cluster, node);
         return FALSE;
      }
      node->needs_auth = FALSE;
   }

   mongoc_cluster_update_state(cluster);

   return TRUE;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_reconnect_replica_set --
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
mongoc_cluster_reconnect_replica_set (mongoc_cluster_t *cluster, /* IN */
                                      bson_error_t     *error)   /* OUT */
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_cluster_node_t node;
   mongoc_host_list_t host;
   mongoc_stream_t *stream;
   mongoc_list_t *list;
   mongoc_list_t *liter;
   bson_uint32_t i;
   bson_int32_t ping;
   const char *replSet;

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->mode == MONGOC_CLUSTER_REPLICA_SET);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      return FALSE;
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

   mongoc_cluster_clear_peers(cluster);

   /*
    * Discover all the potential peers from our seeds.
    */
   for (iter = hosts; iter; iter = iter->next) {
      stream = mongoc_client_create_stream(cluster->client, iter, error);
      if (!stream) {
         MONGOC_WARNING("Failed connection to %s", iter->host_and_port);
         continue;
      }

      mongoc_cluster_node_init(&node);
      node.host = *iter;
      node.stream = stream;

      if (!mongoc_cluster_ismaster(cluster, &node, error)) {
         mongoc_cluster_node_destroy(&node);
         continue;
      }

      if (!node.replSet || !!strcmp(node.replSet, replSet)) {
         MONGOC_INFO("%s: Got replicaSet \"%s\" expected \"%s\".",
                     iter->host_and_port, node.replSet, replSet);
      }

      if (node.primary) {
         mongoc_cluster_node_destroy(&node);
         break;
      }

      mongoc_cluster_node_destroy(&node);
   }

   list = cluster->peers;
   cluster->peers = NULL;

   for (liter = list, i = 0;
        liter && (i < MONGOC_CLUSTER_MAX_NODES);
        liter = liter->next) {

      if (!mongoc_host_list_from_string(&host, liter->data)) {
         MONGOC_WARNING("Failed to parse host and port: \"%s\"",
                        (char *)liter->data);
         continue;
      }

      stream = mongoc_client_create_stream(cluster->client, &host, error);
      if (!stream) {
         MONGOC_WARNING("Failed connection to %s", host.host_and_port);
         continue;
      }

      mongoc_cluster_node_init(&cluster->nodes[i]);
      cluster->nodes[i].host = host;
      cluster->nodes[i].index = i;
      cluster->nodes[i].stream = stream;

      if (!mongoc_cluster_ismaster(cluster, &cluster->nodes[i], error)) {
         mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      if (!cluster->nodes[i].replSet ||
          !!strcmp(cluster->nodes[i].replSet, replSet)) {
         MONGOC_INFO("%s: Got replicaSet \"%s\" expected \"%s\".",
                     host.host_and_port,
                     cluster->nodes[i].replSet,
                     replSet);
         mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      if (-1 == (ping = mongoc_cluster_ping_node(cluster,
                                                 &cluster->nodes[i],
                                                 error))) {
         MONGOC_INFO("%s: Lost connection during ping.",
                     host.host_and_port);
         mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      cluster->nodes[i].ping_msec = ping;

      i++;
   }

   mongoc_list_foreach(list, (void *)bson_free, NULL);
   mongoc_list_destroy(list);

   if (i == 0) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                     "No acceptable peer could be found.");
      return FALSE;
   }

   mongoc_cluster_update_state(cluster);

   return TRUE;
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

static bson_bool_t
mongoc_cluster_reconnect (mongoc_cluster_t *cluster, /* IN */
                          bson_error_t     *error)   /* OUT */
{
   bson_return_val_if_fail(cluster, FALSE);

   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      return mongoc_cluster_reconnect_direct(cluster, error);
   case MONGOC_CLUSTER_REPLICA_SET:
      return mongoc_cluster_reconnect_replica_set(cluster, error);
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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_inc_egress_rpc --
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
mongoc_cluster_inc_egress_rpc (const mongoc_rpc_t *rpc)
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
 * mongoc_cluster_inc_ingress_rpc --
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
mongoc_cluster_inc_ingress_rpc (const mongoc_rpc_t *rpc)
{
   mongoc_counter_op_ingress_total_inc();

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_ingress_delete_inc();
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_ingress_update_inc();
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_ingress_insert_inc();
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_ingress_killcursors_inc();
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_ingress_getmore_inc();
      break;
   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_ingress_reply_inc();
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_ingress_msg_inc();
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_ingress_query_inc();
      break;
   default:
      BSON_ASSERT(FALSE);
      break;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_sendv --
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
mongoc_cluster_sendv (mongoc_cluster_t             *cluster,       /* IN */
                      mongoc_rpc_t                 *rpcs,          /* INOUT */
                      size_t                        rpcs_len,      /* IN */
                      bson_uint32_t                 hint,          /* IN */
                      const mongoc_write_concern_t *write_concern, /* IN */
                      const mongoc_read_prefs_t    *read_prefs,    /* IN */
                      bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *node;
   bson_int64_t now;
   const bson_t *b;
   mongoc_rpc_t gle;
   struct iovec *iov;
   bson_bool_t need_gle;
   size_t iovcnt;
   size_t i;
   int retry_count = 0;

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
      if (!mongoc_cluster_reconnect(cluster, error)) {
         return FALSE;
      }
   }

   /*
    * Try to find a node to deliver to. Since we are allowed to block in this
    * version of sendv, we try to reconnect if we cannot select a node.
    */
   while (!(node = mongoc_cluster_select(cluster,
                                         rpcs,
                                         rpcs_len,
                                         hint,
                                         write_concern,
                                         read_prefs,
                                         error))) {
      if ((retry_count++ == MAX_RETRY_COUNT) ||
          !mongoc_cluster_reconnect(cluster, error)) {
         return FALSE;
      }
   }

   BSON_ASSERT(node->stream);

   mongoc_array_clear(&cluster->iov);

   /*
    * TODO: We can probably remove the need for sendv and just do send since
    * we support write concerns now. Also, we clobber our getlasterror on
    * each subsequent mutation. It's okay, since it comes out correct anyway,
    * just useless work (and technically the request_id changes).
    */

   for (i = 0; i < rpcs_len; i++) {
      mongoc_cluster_inc_egress_rpc(&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = mongoc_rpc_needs_gle(&rpcs[i], write_concern);
      mongoc_rpc_gather(&rpcs[i], &cluster->iov);
      mongoc_rpc_swab(&rpcs[i]);
      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;
         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            gle.query.collection = rpcs[i].insert.collection;
            break;
         case MONGOC_OPCODE_DELETE:
            gle.query.collection = rpcs[i].delete.collection;
            break;
         case MONGOC_OPCODE_UPDATE:
            gle.query.collection = rpcs[i].update.collection;
            break;
         default:
            BSON_ASSERT(FALSE);
            gle.query.collection = "";
            break;
         }
         gle.query.skip = 0;
         gle.query.n_return = 1;
         b = mongoc_write_concern_freeze((void*)write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         mongoc_rpc_gather(&gle, &cluster->iov);
         mongoc_rpc_swab(&gle);
      }
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   BSON_ASSERT(cluster->iov.len);

   if (!mongoc_stream_writev(node->stream, iov, iovcnt,
                             cluster->sockettimeoutms)) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failure during socket delivery: %s",
                     strerror(errno));
      mongoc_cluster_disconnect_node(cluster, node);
      return 0;
   }

   return node->index + 1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_try_sendv --
 *
 *       Deliver an RPC to a remote MongoDB instance.
 *
 *       This function is similar to mongoc_cluster_sendv() except that it
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
mongoc_cluster_try_sendv (
      mongoc_cluster_t             *cluster,       /* IN */
      mongoc_rpc_t                 *rpcs,          /* INOUT */
      size_t                        rpcs_len,      /* IN */
      bson_uint32_t                 hint,          /* IN */
      const mongoc_write_concern_t *write_concern, /* IN */
      const mongoc_read_prefs_t    *read_prefs,    /* IN */
      bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *node;
   struct iovec *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   bson_bool_t need_gle;
   size_t iovcnt;
   size_t i;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(rpcs, FALSE);
   bson_return_val_if_fail(rpcs_len, FALSE);

   if (!(node = mongoc_cluster_select(cluster, rpcs, rpcs_len, hint,
                                      write_concern, read_prefs, error))) {
      return 0;
   }

   BSON_ASSERT(node->stream);

   mongoc_array_clear(&cluster->iov);

   for (i = 0; i < rpcs_len; i++) {
      mongoc_cluster_inc_egress_rpc(&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = mongoc_rpc_needs_gle(&rpcs[i], write_concern);
      mongoc_rpc_gather(&rpcs[i], &cluster->iov);
      mongoc_rpc_swab(&rpcs[i]);
      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;
         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            gle.query.collection = rpcs[i].insert.collection;
            break;
         case MONGOC_OPCODE_DELETE:
            gle.query.collection = rpcs[i].delete.collection;
            break;
         case MONGOC_OPCODE_UPDATE:
            gle.query.collection = rpcs[i].update.collection;
            break;
         default:
            BSON_ASSERT(FALSE);
            gle.query.collection = "";
            break;
         }
         gle.query.skip = 0;
         gle.query.n_return = 1;
         b = mongoc_write_concern_freeze((void *)write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         mongoc_rpc_gather(&gle, &cluster->iov);
         mongoc_rpc_swab(&gle);
      }
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   if (!mongoc_stream_writev(node->stream, iov, iovcnt,
                             cluster->sockettimeoutms)) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failure during socket delivery: %s",
                     strerror(errno));
      mongoc_cluster_disconnect_node(cluster, node);
      return 0;
   }

   return node->index + 1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_try_recv --
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
mongoc_cluster_try_recv (mongoc_cluster_t *cluster, /* IN */
                         mongoc_rpc_t     *rpc,     /* OUT */
                         mongoc_buffer_t  *buffer,  /* INOUT */
                         bson_uint32_t     hint,    /* IN */
                         bson_error_t     *error)   /* OUT */
{
   mongoc_cluster_node_t *node;
   bson_int32_t msg_len;
   off_t pos;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(rpc, FALSE);
   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(hint, FALSE);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, FALSE);

   /*
    * Fetch the node to communicate over.
    */
   node = &cluster->nodes[hint-1];
   if (!node->stream) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Failed to receive message, lost connection to node.");
      return FALSE;
   }

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!mongoc_buffer_append_from_stream(buffer, node->stream, 4,
                                         cluster->sockettimeoutms, error)) {
      mongoc_counter_protocol_ingress_error_inc();
      mongoc_cluster_disconnect_node(cluster, node);
      return FALSE;
   }

   /*
    * Read the msg length from the buffer.
    */
   memcpy(&msg_len, &buffer->data[buffer->off + pos], 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > cluster->max_bson_size)) {
      bson_set_error(error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Corrupt or malicious reply received.");
      mongoc_cluster_disconnect_node(cluster, node);
      mongoc_counter_protocol_ingress_error_inc();
      return FALSE;
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!mongoc_buffer_append_from_stream(buffer, node->stream, msg_len - 4,
                                         cluster->sockettimeoutms, error)) {
      mongoc_cluster_disconnect_node(cluster, node);
      mongoc_counter_protocol_ingress_error_inc();
      return FALSE;
   }

   /*
    * Scatter the buffer into the rpc structure.
    */
   if (!mongoc_rpc_scatter(rpc, &buffer->data[buffer->off + pos], msg_len)) {
      bson_set_error(error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Failed to decode reply from server.");
      mongoc_cluster_disconnect_node(cluster, node);
      mongoc_counter_protocol_ingress_error_inc();
      return FALSE;
   }

   /*
    * Convert endianness of the message.
    */
   mongoc_rpc_swab(rpc);

   mongoc_cluster_inc_ingress_rpc(rpc);

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
/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_stamp --
 *
 *       Returns the stamp of the node provided. The stamp is a monotonic
 *       counter that tracks changes to a node within the cluster. As
 *       changes to the node instance are made, the value is incremented.
 *       This helps cursors and other connection sensitive portions fail
 *       gracefully (or reset) upon loss of connection.
 *
 * Returns:
 *       A 32-bit stamp indiciting the number of times a modification to
 *       the node structure has occured.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_uint32_t
mongoc_cluster_stamp (const mongoc_cluster_t *cluster, /* IN */
                      bson_uint32_t           node)    /* IN */
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= MONGOC_CLUSTER_MAX_NODES, 0);

   return cluster->nodes[node].stamp;
}
