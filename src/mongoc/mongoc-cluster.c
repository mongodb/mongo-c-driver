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
#include "mongoc-scram-private.h"
#include "mongoc-server-selection.h"
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
_mongoc_cluster_node_init (mongoc_cluster_node_t *node, int32_t id)
{
   ENTRY;

   BSON_ASSERT(node);

   memset(node, 0, sizeof *node);

   node->id = id;
   node->stream = NULL;

   // TODO: delete?
   // node->stamp = 0;
   //node->needs_auth = 0;

   EXIT;
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

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_remove_node --
 *
 *       Disconnects and destroys a cluster node.
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
_mongoc_cluster_remove_node (mongoc_cluster_t      *cluster,
                             mongoc_cluster_node_t *node)
{
   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);

   // TODO: remove from cluster's array
   // actually, just move it to the end of the array
   // shift all the other elements down
   // update the "active nodes" index

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_node_by_id --
 *
 *       If node is in the cluster, return it.  Otherwise, return NULL.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static mongoc_cluster_node_t *
_mongoc_cluster_node_by_id (mongoc_cluster_t *cluster, int32_t id)
{
   return NULL;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_add_node --
 *
 *       Add a new node to this cluster for the given server description.
 *
 *       NOTE: does NOT check if this server is already in the cluster.
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
_mongoc_cluster_add_node (mongoc_cluster_t *cluster,
                          mongoc_server_description_t *description,
                          bson_error_t *error /* OUT */)
{
   mongoc_cluster_node_t *node;
   mongoc_cluster_node_t new_node;
   mongoc_stream_t *stream;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(server);

   MONGOC_DEBUG("Adding new server to cluster: %s", description->connection_address);

   stream = _mongoc_client_create_stream(cluster->client, &description->host, error);
   if (!stream) {
      MONGOC_WARNING("Failed connection to %s", description->connection_address);
   }

   /* if we have too many active nodes, we must append */
   if (cluster->active_nodes == cluster->nodes.len) {
      _mongoc_cluster_node_init(&new_node, -1);
      _mongoc_array_append_val(&cluster->nodes, new_node);
   }

   node = &_mongoc_array_index(&cluster->nodes, mongoc_cluster_node_t, cluster->active_nodes);
   node->stream = stream;
   node->id = description->id;
   cluster->active_nodes++;

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_fetch_node --
 *
 *       Given a server description, if we already have it, return it.
 *       Otherwise, add this node to the cluster.
 *
 * Returns:
 *       A mongoc_cluster_node_t, or NULL upon failure, in which case
 *       @error will be set.
 *
 * Side effects:
 *       May increase the size of cluster's array of nodes.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_cluster_node_t *
_mongoc_cluster_fetch_node(mongoc_cluster_t *cluster,
                           mongoc_server_description_t *description,
                           bson_error_t *error /* OUT */)
{
   mongoc_cluster_node_t *node = NULL;

   ENTRY;

   node = _mongoc_cluster_node_by_id(cluster, description->id);
   if (!node) {
      _mongoc_cluster_add_node(cluster, description, error);
      node = _mongoc_cluster_node_by_id(cluster, description->id);
   }

   RETURN(node);
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
   mongoc_cluster_node_t *node;

   ENTRY;

   bson_return_if_fail(cluster);
   bson_return_if_fail(uri);

   memset (cluster, 0, sizeof *cluster);

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
   cluster->sec_latency_ms = 15; // TODO SDAM make configurable?
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = (mongoc_uri_get_username(uri) ||
                             mongoc_uri_get_auth_mechanism(uri));
   cluster->sockettimeoutms = DEFAULT_SOCKET_TIMEOUT_MSEC; // TODO SDAM make configurable?

   _mongoc_array_init (&cluster->iov, sizeof (mongoc_iovec_t));

   /* initialize our buffer of nodes with ids of -1 */
   cluster->active_nodes = 0;
   _mongoc_array_init (&cluster->nodes, sizeof (mongoc_cluster_node_t));
   for (int i = 0; i < cluster->nodes.len; i++) {
      node = &_mongoc_array_index(&cluster->nodes, mongoc_cluster_node_t, i);
      _mongoc_cluster_node_init(node, -1);
   }

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
   mongoc_cluster_node_t *node;
   ENTRY;

   bson_return_if_fail(cluster);

   mongoc_uri_destroy(cluster->uri);

   for (int i = 0; i < cluster->nodes.len; i++) {
      node = &_mongoc_array_index(&cluster->nodes, mongoc_cluster_node_t, i);
      _mongoc_cluster_node_destroy(node);
   }

   _mongoc_array_destroy(&cluster->nodes);
   _mongoc_array_destroy(&cluster->iov);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_preselect --
 *
 * Returns:
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

// TODO SS what does this function actually do?

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_select --
 *
 *       Selects a cluster node that is appropriate for handling the
 *       required set of rpc messages.  Takes read preference into account.
 *
 * Returns:
 *       A mongoc_cluster_node_t if successful, or NULL on failure, in
 *       which case error is also set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cluster_node_t *
_mongoc_cluster_select(mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       //uint32_t                      hint, // TODO SS what is this?
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_pref,
                       bson_error_t                 *error /* OUT */)
{
   mongoc_cluster_node_t *selected_node;
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   mongoc_server_description_t *selected_server;
   mongoc_ss_optype_t optype = MONGOC_SS_READ;

   ENTRY;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);

   /* pick the most restrictive optype */
   for (int i = 0; (i < rpcs_len) && (optype == MONGOC_SS_READ); i++) {
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
            optype = MONGOC_SS_WRITE;
         }
         break;
      case MONGOC_OPCODE_DELETE:
      case MONGOC_OPCODE_INSERT:
      case MONGOC_OPCODE_UPDATE:
      default:
         optype = MONGOC_SS_WRITE;
         break;
      }
   }

   // TODO SS: somebody has to hold on to the topology description.
   // I don't think it should be the cluster object, because it shouldn't
   // have to know about these things.  The cluster monitor has to own the
   // topology description.  Maybe the client does, too?
   selected_server = _mongoc_ss_select(optype,
                                       NULL /* topology description */,
                                       read_pref,
                                       error);

   if (!selected_server) {
      RETURN(NULL);
   }

   selected_node = _mongoc_cluster_fetch_node(cluster, selected_server, error);
   RETURN(selected_node);
}
