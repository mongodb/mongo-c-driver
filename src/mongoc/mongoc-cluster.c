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
#include "mongoc-opcode-private.h"
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
static mongoc_stream_t *
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

   mongoc_set_add(cluster->nodes, description->id, stream);

   RETURN(stream);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_disconnect_node --
 *
 *       Remove a node from the set of nodes. This should be done if
 *       a stream in the set is found to be invalid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Removes node from cluster's set of nodes.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_disconnect_node (mongoc_cluster_t *cluster, uint32_t server_id)
{
   mongoc_set_rm(cluster->nodes, server_id);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_fetch_stream --
 *
 *       Fetch the stream for @server_id.
 *
 *       Returns a mongoc_stream_t on success, NULL on failure, in
 *       which case @error will be set.
 *
 * Returns:
 *       A stream, or NULL
 *
 * Side effects:
 *       May add more streams to @cluster->nodes.
 *       May set @error.
 *
 *--------------------------------------------------------------------------
 */
static mongoc_stream_t *
_mongoc_cluster_fetch_stream (mongoc_cluster_t *cluster,
                              uint32_t server_id,
                              bson_error_t *error)
{
   mongoc_stream_t *stream;

   bson_return_val_if_fail(cluster, NULL);

   stream = mongoc_set_get(cluster->nodes, server_id);
   if (stream) {
      return stream;
   }

   // TODO: do we want to try to get the server description here and reconnect?
   bson_set_error(error,
                  MONGOC_ERROR_STREAM,
                  MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                  "No stream available for server_id %ul", server_id);

   _mongoc_cluster_disconnect_node(cluster, server_id);

   return NULL;
}


static void
_mongoc_cluster_stream_dtor (void *stream_,
                             void *ctx_)
{
   mongoc_stream_destroy ((mongoc_stream_t *)stream_);
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
   cluster->nodes = mongoc_set_new(8, _mongoc_cluster_stream_dtor, NULL);

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
   ENTRY;

   bson_return_if_fail(cluster);

   mongoc_uri_destroy(cluster->uri);

   mongoc_set_destroy(cluster->nodes);

   _mongoc_array_destroy(&cluster->iov);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_select_by_optype --
 *
 *       Internal server selection.
 *
 *       NOTE: caller becomes the owner of returned server description
 *       and must clean it up.
 *
 *
 *--------------------------------------------------------------------------
 */
mongoc_server_description_t *
_mongoc_cluster_select_by_optype(mongoc_cluster_t *cluster,
                                 mongoc_ss_optype_t optype,
                                 const mongoc_write_concern_t *write_concern,
                                 const mongoc_read_prefs_t    *read_prefs,
                                 bson_error_t                 *error)
{
   mongoc_stream_t *stream;
   mongoc_server_description_t *selected_server;

   ENTRY;

   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(optype, 0);
   bson_return_val_if_fail(write_concern, 0);
   bson_return_val_if_fail(read_prefs, 0);

   selected_server = _mongoc_sdam_select(cluster->client->sdam,
                                         optype,
                                         read_prefs,
                                         error);

   if (!selected_server) {
      RETURN(NULL);
   }

   /* pre-load this stream if we don't already have it */
   stream = mongoc_set_get (cluster->nodes, selected_server->id);
   if (!stream) {
      // TODO: error handling, if we can't add stream should we still return
      // this server description?
      stream = _mongoc_cluster_add_node (cluster, selected_server, error);
   }

   RETURN(selected_server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_preselect_description --
 *
 *       Server selection by opcode, with retries, returns full
 *       server description.
 *
 *       NOTE: caller becomes the owner of returned server description
 *       and must clean it up.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL on failure (sets @error)
 *
 * Side effects:
 *       May set @error.
 *       May add new nodes to @cluster->nodes.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_cluster_preselect_description (mongoc_cluster_t             *cluster,
                                       mongoc_opcode_t               opcode,
                                       const mongoc_write_concern_t *write_concern,
                                       const mongoc_read_prefs_t    *read_prefs,
                                       bson_error_t                 *error /* OUT */)
{
   int retry_count = 0;
   mongoc_server_description_t *server;
   mongoc_read_mode_t read_mode;
   mongoc_ss_optype_t optype = MONGOC_SS_READ;

   if (_mongoc_opcode_needs_primary(opcode)) {
      optype = MONGOC_SS_WRITE;
   }

   /* we can run queries on secondaries if given the right read mode */
   if (optype == MONGOC_SS_WRITE &&
       opcode == MONGOC_OPCODE_QUERY) {
      read_mode = mongoc_read_prefs_get_mode(read_prefs);
      if ((read_mode & MONGOC_READ_SECONDARY) != 0) {
         optype = MONGOC_SS_READ;
      }
   }

   while (retry_count++ < MAX_RETRY_COUNT) {
      server = _mongoc_cluster_select_by_optype(cluster, optype, write_concern,
                                                read_prefs, error);
      if (server) {
         break;
      }
   }

   return server;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_preselect --
 *
 *       Server selection by opcode with retries.
 *
 *--------------------------------------------------------------------------
 */
uint32_t
_mongoc_cluster_preselect(mongoc_cluster_t             *cluster,
                          mongoc_opcode_t               opcode,
                          const mongoc_write_concern_t *write_concern,
                          const mongoc_read_prefs_t    *read_prefs,
                          bson_error_t                 *error)
{
   mongoc_server_description_t *server;
   uint32_t server_id;

   server = _mongoc_cluster_preselect_description(cluster, opcode, write_concern,
                                                  read_prefs, error);
   if (server) {
      server_id = server->id;
      _mongoc_server_description_destroy(server);
      return server_id;
   }

   return 0;
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
 *       A server description's id (> 0) if successful, or 0 on failure, in
 *       which case error is also set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_select(mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_prefs,
                       bson_error_t                 *error /* OUT */)
{
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   mongoc_ss_optype_t optype = MONGOC_SS_READ;
   mongoc_opcode_t opcode;
   mongoc_server_description_t *server;
   uint32_t server_id;
   int i;

   ENTRY;

   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(rpcs, 0);
   bson_return_val_if_fail(rpcs_len, 0);

   /* pick the most restrictive optype */
   for (i = 0; (i < rpcs_len) && (optype == MONGOC_SS_READ); i++) {
      opcode = rpcs[i].header.opcode;
      if (_mongoc_opcode_needs_primary(opcode)) {
         /* we can run queries on secondaries if given either:
          * - a read mode of secondary
          * - query flags where slave ok is set */
         if (opcode == MONGOC_OPCODE_QUERY) {
            read_mode = mongoc_read_prefs_get_mode(read_prefs);
            if ((read_mode & MONGOC_READ_SECONDARY) != 0 ||
                (rpcs[i].query.flags & MONGOC_QUERY_SLAVE_OK)) {
               optype = MONGOC_SS_READ;
            }
         }
         else {
            optype = MONGOC_SS_WRITE;
         }
      }
   }

   server = _mongoc_cluster_select_by_optype(cluster, optype, write_concern,
                                             read_prefs, error);
   if (server) {
      server_id = server->id;
      _mongoc_server_description_destroy(server);
      return server_id;
   }
   return 0;
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
 * _mongoc_client_sendv_to_server --
 *
 *       Sends the given RPCs to the given server. On success,
 *       returns the server id of the server to which the messages were
 *       sent. Otherwise, returns 0 and sets error.
 *
 * Returns:
 *       Server id, or 0.
 *
 * Side effects:
 *       @rpcs may be mutated and should be considered invalid after calling
 *       this method.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_sendv_to_server (mongoc_cluster_t              *cluster,
                                 mongoc_rpc_t                  *rpcs,
                                 size_t                         rpcs_len,
                                 uint32_t                       server_id,
                                 const mongoc_write_concern_t  *write_concern,
                                 bson_error_t                  *error)
{
   mongoc_stream_t *stream;
   mongoc_iovec_t *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   size_t iovcnt;
   size_t i;
   bool need_gle;
   char cmdname[140];

   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(rpcs, 0);
   bson_return_val_if_fail(rpcs_len, 0);
   bson_return_val_if_fail(write_concern, 0);
   bson_return_val_if_fail(server_id, 0);
   bson_return_val_if_fail(server_id < 1, 0);

   // TODO: introduce retries

   /*
    * Fetch the stream to communicate over.
    */

   stream = _mongoc_cluster_fetch_stream(cluster, server_id, error);
   if (!stream) {
      return false;
   }

   _mongoc_array_clear(&cluster->iov);

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
         RETURN(false);
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

   BSON_ASSERT(cluster->iov.len);

   if (!mongoc_stream_writev (stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      char buf[128];
      char * errstr;
      errstr = bson_strerror_r(errno, buf, sizeof buf);

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      errstr);
      mongoc_set_rm(cluster->nodes, server_id);
      return false;
   }

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_sendv --
 *
 *       Sends the given RPCs to an appropriate server. On success,
 *       returns the server id of the server to which the messages were
 *       sent. Otherwise, returns 0 and sets error.
 *
 * Returns:
 *       Server id, or 0.
 *
 * Side effects:
 *       @rpcs may be mutated and should be considered invalid after calling
 *       this method.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_sendv (mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_prefs,
                       bson_error_t                 *error)
{
   uint32_t server_id;

   server_id = _mongoc_cluster_select(cluster, rpcs, rpcs_len,
                                            write_concern, read_prefs, error);
   if (server_id < 1) {
      return server_id;
   }

   if(_mongoc_cluster_sendv_to_server(cluster, rpcs, rpcs_len, server_id,
                                      write_concern, error)) {
      return true;
   }
   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_try_recv --
 *
 *       Tries to receive the next event from the MongoDB server
 *       specified by @server_id. The contents are loaded into @buffer and then
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
                          uint32_t          server_id,
                          bson_error_t     *error)
{
   mongoc_stream_t *stream;
   int32_t msg_len;
   off_t pos;

   ENTRY;

   bson_return_val_if_fail (cluster, false);
   bson_return_val_if_fail (rpc, false);
   bson_return_val_if_fail (buffer, false);
   bson_return_val_if_fail (server_id, false);
   bson_return_val_if_fail (server_id < 1, false);

   /*
    * Fetch the stream to communicate over.
    */
   stream = _mongoc_cluster_fetch_stream(cluster, server_id, error);
   if (!stream) {
      RETURN (false);
   }

   TRACE ("Waiting for reply from server \"%ul\"", server_id);

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!_mongoc_buffer_append_from_stream (buffer, stream, 4,
                                           cluster->sockettimeoutms, error)) {
      mongoc_counter_protocol_ingress_error_inc ();
      _mongoc_cluster_disconnect_node(cluster, server_id);
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
      _mongoc_cluster_disconnect_node(cluster, server_id);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!_mongoc_buffer_append_from_stream (buffer, stream, msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      _mongoc_cluster_disconnect_node (cluster, server_id);
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
      _mongoc_cluster_disconnect_node (cluster, server_id);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   DUMP_BYTES (buffer, buffer->data + buffer->off, buffer->len);

   _mongoc_rpc_swab_from_le (rpc);

   _mongoc_cluster_inc_ingress_rpc (rpc);

   RETURN(true);
}
