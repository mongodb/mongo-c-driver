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
 * mongoc_cluster_force_scan --
 *
 *       Check all nodes to update the cluster state.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       May update cluster and server state.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_force_scan (mongoc_cluster_t *cluster)
{
   // TODO, when Jason is done
   return;
}

// TODO:
// returns -1 if not found, otherwise returns array index of node
static int32_t
_mongoc_cluster_node_by_id (mongoc_cluster_t *cluster, int32_t id) {
   return -1;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_add_node --
 *
 *       Add a new node to this cluster.
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
                          mongoc_server_description_t *description)
{
   mongoc_cluster_node_t *node;
   mongoc_cluster_node_t new_node;
   bson_error_t error; // TODO: should we take this error from somewhere else?
   mongoc_stream_t *stream;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(server);

   if (_mongoc_cluster_node_by_id(cluster, description->id) > -1) {
      return;
   }

   MONGOC_DEBUG("Adding new server to cluster: %s", description->connection_address);

   stream = _mongoc_client_create_stream(cluster->client, &description->host, &error);
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

// TODO:

// change cluster add node

// server selection method

// change cluster remove node
