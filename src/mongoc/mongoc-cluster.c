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
#include "mongoc-sdam-private.h"
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
mongoc_stream_t *
_mongoc_cluster_add_node (mongoc_cluster_t *cluster,
                          mongoc_server_description_t *description,
                          bson_error_t *error /* OUT */)
{
   mongoc_stream_t *stream;

   ENTRY;

   BSON_ASSERT(cluster);

   MONGOC_DEBUG("Adding new server to cluster: %s", description->connection_address);

   stream = _mongoc_client_create_stream(cluster->client, &description->host, error);
   if (!stream) {
      MONGOC_WARNING("Failed connection to %s", description->connection_address);
      RETURN(NULL);
   }

   mongoc_node_switch_add(cluster->node_switch, description->id, stream);

   RETURN(stream);
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
   cluster->node_switch = mongoc_node_switch_new();

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
   int i;
   ENTRY;

   bson_return_if_fail(cluster);

   mongoc_uri_destroy(cluster->uri);

   mongoc_node_switch_destroy(cluster->node_switch);

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

uint32_t
_mongoc_cluster_preselect (mongoc_collection_t          *collection,
                              mongoc_opcode_t               opcode,
                              const mongoc_write_concern_t *write_concern,
                              const mongoc_read_prefs_t    *read_prefs,
                              uint32_t                     *min_wire_version,
                              uint32_t                     *max_wire_version,
                              bson_error_t                 *error)
{
   uint32_t hint;

   ENTRY;

   hint = 0;

   /*

   stream = mongoc_node_switch_get (cluster->node_switch, selected_server->id);

   if (! stream) {
      stream = _mongoc_cluster_add_node (cluster, selected_server, error);
   }
   */

   if (hint) {
   }

   return hint;
}

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

mongoc_stream_t *
_mongoc_cluster_select(mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       //uint32_t                      hint, // TODO SS what is this?
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_pref,
                       bson_error_t                 *error /* OUT */)
{
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   mongoc_server_description_t *selected_server;
   mongoc_ss_optype_t optype = MONGOC_SS_READ;
   mongoc_stream_t *stream;
   int i;

   ENTRY;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);

   /* pick the most restrictive optype */
   for (i = 0; (i < rpcs_len) && (optype == MONGOC_SS_READ); i++) {
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

   selected_server = _mongoc_sdam_select(cluster->client->sdam,
                                         optype,
                                         read_pref,
                                         error);

   if (!selected_server) {
      RETURN(NULL);
   }

   stream = mongoc_node_switch_get (cluster->node_switch, selected_server->id);

   if (! stream) {
      stream = _mongoc_cluster_add_node (cluster, selected_server, error);
   }

   RETURN(stream);
}
