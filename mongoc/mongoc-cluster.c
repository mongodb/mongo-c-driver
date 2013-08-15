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
#include "mongoc-util-private.h"
#include "mongoc-write-concern-private.h"


#ifndef MAX_RETRY_COUNT
#define MAX_RETRY_COUNT 3
#endif


static char *
mongoc_cluster_build_basic_auth_digest (mongoc_cluster_t *cluster,
                                        const char       *nonce)
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


static void
mongoc_cluster_disconnect_node (mongoc_cluster_t      *cluster,
                                mongoc_cluster_node_t *node)
{
   mongoc_stream_t *stream;

   bson_return_if_fail(node);

   stream = node->stream;
   node->stream = NULL;
   node->needs_auth = cluster->requires_auth;
   node->ping_msec = -1;
   node->stamp = 0;
   node->primary = 0;
   bson_destroy(&node->tags);
   bson_init(&node->tags);

   mongoc_stream_close(stream);
   mongoc_stream_destroy(stream);
}


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
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = !!mongoc_uri_get_username(uri);

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_msec = -1;
      cluster->nodes[i].needs_auth = cluster->requires_auth;
      bson_init(&cluster->nodes[i].tags);
   }

   mongoc_array_init(&cluster->iov, sizeof(struct iovec));
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

   mongoc_array_destroy(&cluster->iov);
}


static mongoc_cluster_node_t *
mongoc_cluster_select (mongoc_cluster_t       *cluster,
                       mongoc_rpc_t           *rpcs,
                       size_t                  rpcs_len,
                       bson_uint32_t           hint,
                       mongoc_write_concern_t *write_concern,
                       mongoc_read_prefs_t    *read_prefs,
                       bson_error_t           *error)
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   bson_uint32_t count;
   bson_uint32_t watermark;
   bson_int32_t nearest = -1;
   bson_bool_t need_primary;
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
    * Determine if our read preference requires communicating with PRIMARY.
    */
   if (read_prefs) {
      read_mode = mongoc_read_prefs_get_mode(read_prefs);
   }
   need_primary = (read_mode == MONGOC_READ_PRIMARY);

   /*
    * Check to see if any RPCs require the primary. If so, we pin all
    * of the RPCs to the primary.
    */
   for (i = 0; !need_primary && i < rpcs_len; i++) {
      switch (rpcs[i].header.opcode) {
      case MONGOC_OPCODE_KILL_CURSORS:
      case MONGOC_OPCODE_GET_MORE:
      case MONGOC_OPCODE_MSG:
      case MONGOC_OPCODE_REPLY:
         break;
      case MONGOC_OPCODE_QUERY:
         if (!(rpcs[i].query.flags & MONGOC_QUERY_SLAVE_OK)) {
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
#if 0
         if (read_prefs) {
         }
#endif
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
mongoc_cluster_node_run_command (mongoc_cluster_t      *cluster,
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

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);
   BSON_ASSERT(db_name);
   BSON_ASSERT(command);

   snprintf(ns, sizeof ns, "%s.$cmd", db_name);
   ns[sizeof ns - 1] = '\0';

   rpc.query.msg_len = 0;
   rpc.query.request_id = ++cluster->request_id;
   rpc.query.response_to = -1;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_NONE;
   rpc.query.collection = ns;
   rpc.query.skip = 0;
   rpc.query.n_return = -1;
   rpc.query.query = bson_get_data(command);
   rpc.query.fields = NULL;

   mongoc_array_init(&ar, sizeof(struct iovec));
   mongoc_buffer_init(&buffer, NULL, 0, NULL);
   mongoc_rpc_gather(&rpc, &ar);
   mongoc_rpc_swab(&rpc);

   if (!mongoc_stream_writev(node->stream, ar.data, ar.len, 0)) {
      goto failure;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, 4, error)) {
      goto failure;
   }

   BSON_ASSERT(buffer.len == 4);

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > (1024 * 1024 * 16))) {
      goto invalid_reply;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, msg_len - 4, error)) {
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

   return FALSE;
}


static bson_bool_t
mongoc_cluster_ismaster (mongoc_cluster_t      *cluster,
                         mongoc_cluster_node_t *node,
                         bson_error_t          *error)
{
   mongoc_buffer_t buffer;
   mongoc_array_t ar;
   mongoc_rpc_t rpc;
   bson_int32_t msg_len;
   bson_iter_t iter;
   bson_t q;
   bson_t r;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&q);
   bson_append_int32(&q, "ismaster", 8, 1);

   rpc.query.msg_len = 0;
   rpc.query.request_id = ++cluster->request_id;
   rpc.query.response_to = -1;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_NONE;
   rpc.query.collection = "admin.$cmd";
   rpc.query.skip = 0;
   rpc.query.n_return = 1;
   rpc.query.query = bson_get_data(&q);
   rpc.query.fields = NULL;

   mongoc_array_init(&ar, sizeof(struct iovec));
   mongoc_buffer_init(&buffer, NULL, 0, NULL);
   mongoc_rpc_gather(&rpc, &ar);
   mongoc_rpc_swab(&rpc);

   if (!mongoc_stream_writev(node->stream, ar.data, ar.len, 0)) {
      goto failure;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, 4, error)) {
      goto failure;
   }

   BSON_ASSERT(buffer.len == 4);

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > (1024 * 1024 * 16))) {
      goto invalid_reply;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, node->stream, msg_len - 4, error)) {
      goto failure;
   }

   if (!mongoc_rpc_scatter(&rpc, buffer.data, buffer.len)) {
      goto invalid_reply;
   }

   mongoc_rpc_swab(&rpc);

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      goto invalid_reply;
   }

   if (mongoc_rpc_reply_get_first(&rpc.reply, &r)) {
      if (bson_iter_init_find_case(&iter, &r, "isMaster") &&
          BSON_ITER_HOLDS_BOOL(&iter) &&
          bson_iter_bool(&iter)) {
         node->primary = TRUE;
      }

      if (bson_iter_init_find_case(&iter, &r, "maxMessageSizeBytes")) {
         cluster->max_msg_size = bson_iter_int32(&iter);
      }

      if (bson_iter_init_find_case(&iter, &r, "maxBsonObjectSize")) {
         cluster->max_bson_size = bson_iter_int32(&iter);
      }

      bson_destroy(&r);
   }

   mongoc_buffer_destroy(&buffer);
   mongoc_array_destroy(&ar);
   bson_destroy(&q);

   return TRUE;

invalid_reply:
   bson_set_error(error,
                  MONGOC_ERROR_PROTOCOL,
                  MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                  "Invalid reply from server.");

failure:
   mongoc_buffer_destroy(&buffer);
   mongoc_array_destroy(&ar);
   bson_destroy(&q);

   return FALSE;
}


static bson_bool_t
mongoc_cluster_auth_node (mongoc_cluster_t      *cluster,
                          mongoc_cluster_node_t *node,
                          bson_error_t          *error)
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
   if (!mongoc_cluster_node_run_command(cluster,
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
   if (!mongoc_cluster_node_run_command(cluster,
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
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_AUTHENTICATE,
                     "Failed to authenticate credentials.");
      bson_destroy(&reply);
      return FALSE;
   }
   bson_destroy(&reply);

   return TRUE;
}


static bson_bool_t
mongoc_cluster_reconnect_direct (mongoc_cluster_t *cluster,
                                 bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   mongoc_cluster_node_t *node;
   mongoc_stream_t *stream;

   bson_return_val_if_fail(cluster, FALSE);
   bson_return_val_if_fail(error, FALSE);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      return FALSE;
   }

   node = &cluster->nodes[0];

   node->index = 0;
   node->host = *hosts;
   node->needs_auth = cluster->requires_auth;
   node->primary = FALSE;
   node->ping_msec = -1;
   node->stream = NULL;
   node->stamp = 0;
   bson_init(&node->tags);

   stream = mongoc_client_create_stream(cluster->client, hosts, error);
   if (!stream) {
      return FALSE;
   }

   node->stream = stream;
   node->stamp++;

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


static bson_bool_t
mongoc_cluster_needs_gle (mongoc_cluster_t       *cluster,
                          mongoc_rpc_t           *rpc,
                          mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT(cluster);
   BSON_ASSERT(rpc);

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
   case MONGOC_OPCODE_QUERY:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_KILL_CURSORS:
      return FALSE;
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   case MONGOC_OPCODE_DELETE:
   default:
      break;
   }

   if (!write_concern || !mongoc_write_concern_get_w(write_concern)) {
      return FALSE;
   }

   return TRUE;
}


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
   }
}


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
   }
}


bson_uint32_t
mongoc_cluster_sendv (mongoc_cluster_t       *cluster,
                      mongoc_rpc_t           *rpcs,
                      size_t                  rpcs_len,
                      bson_uint32_t           hint,
                      mongoc_write_concern_t *write_concern,
                      mongoc_read_prefs_t    *read_prefs,
                      bson_error_t           *error)
{
   mongoc_cluster_node_t *node;
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
    * Try to find a node to deliver to. Since we are allowed to block in this
    * version of sendv, we try to reconnect if we cannot select a node.
    */
   while (!(node = mongoc_cluster_select(cluster, rpcs, rpcs_len, hint,
                                         write_concern, read_prefs, error))) {
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
      need_gle = mongoc_cluster_needs_gle(cluster, &rpcs[i], write_concern);
      mongoc_rpc_gather(&rpcs[i], &cluster->iov);
      mongoc_rpc_swab(&rpcs[i]);
      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = -1;
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
         b = mongoc_write_concern_freeze(write_concern);
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

   if (!mongoc_stream_writev(node->stream, iov, iovcnt, 0)) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failure during socket delivery: %s",
                     strerror(errno));
      return 0;
   }

   return node->index + 1;
}


bson_uint32_t
mongoc_cluster_try_sendv (mongoc_cluster_t       *cluster,
                          mongoc_rpc_t           *rpcs,
                          size_t                  rpcs_len,
                          bson_uint32_t           hint,
                          mongoc_write_concern_t *write_concern,
                          mongoc_read_prefs_t    *read_prefs,
                          bson_error_t           *error)
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
      need_gle = mongoc_cluster_needs_gle(cluster, &rpcs[i], write_concern);
      mongoc_rpc_gather(&rpcs[i], &cluster->iov);
      mongoc_rpc_swab(&rpcs[i]);
      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = -1;
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
         b = mongoc_write_concern_freeze(write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         mongoc_rpc_gather(&gle, &cluster->iov);
         mongoc_rpc_swab(&gle);
      }
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   if (!mongoc_stream_writev(node->stream, iov, iovcnt, 0)) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failure during socket delivery: %s",
                     strerror(errno));
      return 0;
   }

   return node->index + 1;
}


/**
 * bson_cluster_try_recv:
 * @cluster: A bson_cluster_t.
 * @rpc: (out): A mongoc_rpc_t to scatter into.
 * @buffer: (inout): A mongoc_buffer_t to fill with contents.
 * @hint: The node to receive from, returned from mongoc_cluster_send().
 * @error: (out): A location for a bson_error_t or NULL.
 *
 * Tries to receive the next event from the node in the cluster specified by
 * @hint. The contents are loaded into @buffer and then scattered into the
 * @rpc structure. @rpc is valid as long as @buffer contains the contents
 * read into it.
 *
 * Returns: TRUE if an event was read, otherwise FALSE and @error is set.
 */
bson_bool_t
mongoc_cluster_try_recv (mongoc_cluster_t *cluster,
                         mongoc_rpc_t     *rpc,
                         mongoc_buffer_t  *buffer,
                         bson_uint32_t     hint,
                         bson_error_t     *error)
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
   if (!mongoc_buffer_append_from_stream(buffer, node->stream, 4, error)) {
      mongoc_cluster_disconnect_node(cluster, node);
      mongoc_counter_protocol_ingress_error_inc();
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
   if (!mongoc_buffer_append_from_stream(buffer, node->stream, msg_len - 4, error)) {
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
bson_uint32_t
mongoc_cluster_stamp (mongoc_cluster_t *cluster,
                      bson_uint32_t     node)
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= MONGOC_CLUSTER_MAX_NODES, 0);

   return cluster->nodes[node].stamp;
}
