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


#include <bson.h>
#ifndef _WIN32
# include <netdb.h>
# include <netinet/tcp.h>
#endif

#include "mongoc-cursor-array-private.h"
#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-collection-private.h"
#include "mongoc-config.h"
#include "mongoc-counters-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-database-private.h"
#include "mongoc-gridfs-private.h"
#include "mongoc-error.h"
#include "mongoc-list-private.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-queue-private.h"
#include "mongoc-socket.h"
#include "mongoc-stream-buffered.h"
#include "mongoc-stream-socket.h"
#include "mongoc-thread-private.h"
#include "mongoc-trace.h"

#ifdef MONGOC_ENABLE_SSL
#include "mongoc-stream-tls.h"
#include "mongoc-ssl-private.h"
#endif


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client"


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_connect_tcp --
 *
 *       Connect to a host using a TCP socket.
 *
 *       This will be performed synchronously and return a mongoc_stream_t
 *       that can be used to connect with the remote host.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_connect_tcp (const mongoc_uri_t       *uri,
                           const mongoc_host_list_t *host,
                           bson_error_t             *error)
{
   mongoc_socket_t *sock = NULL;
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   int32_t connecttimeoutms = MONGOC_DEFAULT_CONNECTTIMEOUTMS;
   int64_t expire_at;
   const bson_t *options;
   bson_iter_t iter;
   char portstr [8];
   int s;

   ENTRY;

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

   if ((options = mongoc_uri_get_options (uri)) &&
       bson_iter_init_find_case (&iter, options, "connecttimeoutms") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      if (!(connecttimeoutms = bson_iter_int32(&iter))) {
         connecttimeoutms = MONGOC_DEFAULT_CONNECTTIMEOUTMS;
      }
   }

   BSON_ASSERT (connecttimeoutms);
   expire_at = bson_get_monotonic_time () + (connecttimeoutms * 1000L);

   bson_snprintf (portstr, sizeof portstr, "%hu", host->port);

   memset (&hints, 0, sizeof hints);
   hints.ai_family = host->family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = 0;
   hints.ai_protocol = 0;

   s = getaddrinfo (host->host, portstr, &hints, &result);

   if (s != 0) {
      mongoc_counter_dns_failure_inc ();
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                     "Failed to resolve %s",
                     host->host);
      RETURN (NULL);
   }

   mongoc_counter_dns_success_inc ();

   for (rp = result; rp; rp = rp->ai_next) {
      /*
       * Create a new non-blocking socket.
       */
      if (!(sock = mongoc_socket_new (rp->ai_family,
                                      rp->ai_socktype,
                                      rp->ai_protocol))) {
         continue;
      }

      /*
       * Try to connect to the peer.
       */
      if (0 != mongoc_socket_connect (sock,
                                      rp->ai_addr,
                                      (socklen_t)rp->ai_addrlen,
                                      expire_at)) {
         char *errmsg;
         char errmsg_buf[BSON_ERROR_BUFFER_SIZE];
         char ip[255];

         mongoc_socket_inet_ntop (rp, ip, sizeof ip);
         errmsg = bson_strerror_r (
            mongoc_socket_errno (sock), errmsg_buf, sizeof errmsg_buf);
         MONGOC_WARNING ("Failed to connect to: %s:%d, error: %d, %s\n",
                         ip,
                         host->port,
                         mongoc_socket_errno(sock),
                         errmsg);
         mongoc_socket_destroy (sock);
         sock = NULL;
         continue;
      }

      break;
   }

   if (!sock) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      "Failed to connect to target host: %s",
                      host->host_and_port);
      freeaddrinfo (result);
      RETURN (NULL);
   }

   freeaddrinfo (result);

   return mongoc_stream_socket_new (sock);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_connect_unix --
 *
 *       Connect to a MongoDB server using a UNIX domain socket.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_connect_unix (const mongoc_uri_t       *uri,
                            const mongoc_host_list_t *host,
                            bson_error_t             *error)
{
#ifdef _WIN32
   ENTRY;
   bson_set_error (error,
                   MONGOC_ERROR_STREAM,
                   MONGOC_ERROR_STREAM_CONNECT,
                   "UNIX domain sockets not supported on win32.");
   RETURN (NULL);
#else
   struct sockaddr_un saddr;
   mongoc_socket_t *sock;
   mongoc_stream_t *ret = NULL;

   ENTRY;

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

   memset (&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   bson_snprintf (saddr.sun_path, sizeof saddr.sun_path - 1,
                  "%s", host->host);

   sock = mongoc_socket_new (AF_UNIX, SOCK_STREAM, 0);

   if (sock == NULL) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failed to create socket.");
      RETURN (NULL);
   }

   if (-1 == mongoc_socket_connect (sock,
                                    (struct sockaddr *)&saddr,
                                    sizeof saddr,
                                    -1)) {
      mongoc_socket_destroy (sock);
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      "Failed to connect to UNIX domain socket.");
      RETURN (NULL);
   }

   ret = mongoc_stream_socket_new (sock);

   RETURN (ret);
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_default_stream_initiator --
 *
 *       A mongoc_stream_initiator_t that will handle the various type
 *       of supported sockets by MongoDB including TCP and UNIX.
 *
 *       Language binding authors may want to implement an alternate
 *       version of this method to use their native stream format.
 *
 * Returns:
 *       A mongoc_stream_t if successful; otherwise NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,
                                        const mongoc_host_list_t *host,
                                        void                     *user_data,
                                        bson_error_t             *error)
{
   mongoc_stream_t *base_stream = NULL;
#ifdef MONGOC_ENABLE_SSL
   mongoc_client_t *client = user_data;
   const bson_t *options;
   bson_iter_t iter;
   const char *mechanism;
#endif

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

#ifndef MONGOC_ENABLE_SSL
   if (mongoc_uri_get_ssl (uri)) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "SSL is not enabled in this build of mongo-c-driver.");
      return NULL;
   }
#endif


   switch (host->family) {
#if defined(AF_INET6)
   case AF_INET6:
#endif
   case AF_INET:
      base_stream = mongoc_client_connect_tcp (uri, host, error);
      break;
   case AF_UNIX:
      base_stream = mongoc_client_connect_unix (uri, host, error);
      break;
   default:
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_INVALID_TYPE,
                      "Invalid address family: 0x%02x", host->family);
      break;
   }

#ifdef MONGOC_ENABLE_SSL
   if (base_stream) {
      options = mongoc_uri_get_options (uri);
      mechanism = mongoc_uri_get_auth_mechanism (uri);

      if ((bson_iter_init_find_case (&iter, options, "ssl") &&
           bson_iter_as_bool (&iter)) ||
          (mechanism && (0 == strcmp (mechanism, "MONGODB-X509")))) {
         base_stream = mongoc_stream_tls_new (base_stream, &client->ssl_opts,
                                              true);

         if (!base_stream) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "Failed initialize TLS state.");
            return NULL;
         }

         if (!mongoc_stream_tls_do_handshake (base_stream, -1) ||
             !mongoc_stream_tls_check_cert (base_stream, host->host)) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "Failed to handshake and validate TLS certificate.");
            mongoc_stream_destroy (base_stream);
            base_stream = NULL;
            return NULL;
         }
      }
   }
#endif

   return base_stream ? mongoc_stream_buffered_new (base_stream, 1024) : NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_create_stream --
 *
 *       INTERNAL API
 *
 *       This function is used by the mongoc_cluster_t to initiate a
 *       new stream. This is done because cluster is private API and
 *       those using mongoc_client_t may need to override this process.
 *
 *       This function calls the default initiator for new streams.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
_mongoc_client_create_stream (mongoc_client_t          *client,
                              const mongoc_host_list_t *host,
                              bson_error_t             *error)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(host, NULL);

   return client->initiator (client->uri, host, client->initiator_data, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_sendv --
 *
 *       INTERNAL API
 *
 *       This function is used to deliver one or more RPCs to the remote
 *       MongoDB server.
 *
 *       Based on the cluster state and operation type, the request may
 *       be retried. This is handled by the cluster instance.
 *
 * Returns:
 *       0 upon failure and @error is set. Otherwise non-zero indicating
 *       the cluster node that performed the request.
 *
 * Side effects:
 *       @error is set if return value is 0.
 *       @rpcs is mutated and therefore invalid after calling.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_client_sendv (mongoc_client_t              *client,
                      mongoc_rpc_t                 *rpcs,
                      size_t                        rpcs_len,
                      uint32_t                      hint,
                      const mongoc_write_concern_t *write_concern,
                      const mongoc_read_prefs_t    *read_prefs,
                      bson_error_t                 *error)
{
   size_t i;

   bson_return_val_if_fail(client, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   if (client->in_exhaust) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_IN_EXHAUST,
                     "A cursor derived from this client is in exhaust.");
      RETURN(false);
   }

   for (i = 0; i < rpcs_len; i++) {
      rpcs[i].header.msg_len = 0;
      rpcs[i].header.request_id = ++client->request_id;
   }

   switch (client->cluster.state) {
   case MONGOC_CLUSTER_STATE_BORN:
      return _mongoc_cluster_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                   write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_HEALTHY:
   case MONGOC_CLUSTER_STATE_UNHEALTHY:
      return _mongoc_cluster_try_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                       write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_DEAD:
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "No healthy connections.");
      return false;
   default:
      BSON_ASSERT(false);
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_recv --
 *
 *       Receives a RPC from a remote MongoDB cluster node. @hint should
 *       be the result from a previous call to mongoc_client_sendv() to
 *       signify which node to recv from.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set if return value is false.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_client_recv (mongoc_client_t *client,
                     mongoc_rpc_t    *rpc,
                     mongoc_buffer_t *buffer,
                     uint32_t         hint,
                     bson_error_t    *error)
{
   bson_return_val_if_fail(client, false);
   bson_return_val_if_fail(rpc, false);
   bson_return_val_if_fail(buffer, false);
   bson_return_val_if_fail(hint, false);
   bson_return_val_if_fail(hint <= client->cluster.nodes_len, false);

   return _mongoc_cluster_try_recv (&client->cluster, rpc, buffer, hint,
                                    error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_to_error --
 *
 *       A helper routine to convert a bson document to a bson_error_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @error is set if non-null.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_to_error (const bson_t *b,
                bson_error_t *error)
{
   bson_iter_t iter;
   int code = 0;

   BSON_ASSERT(b);

   if (!error) {
      return;
   }

   if (bson_iter_init_find(&iter, b, "code") && BSON_ITER_HOLDS_INT32(&iter)) {
      code = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find(&iter, b, "$err") && BSON_ITER_HOLDS_UTF8(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s",
                     bson_iter_utf8(&iter, NULL));
      return;
   }

   if (bson_iter_init_find(&iter, b, "errmsg") && BSON_ITER_HOLDS_UTF8(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s",
                     bson_iter_utf8(&iter, NULL));
      return;
   }

   bson_set_error(error,
                  MONGOC_ERROR_QUERY,
                  MONGOC_ERROR_QUERY_FAILURE,
                  "An unknown error ocurred on the server.");
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_recv_gle --
 *
 *       INTERNAL API
 *
 *       This function is used to receive the next RPC from a cluster
 *       node, expecting it to be the response to a getlasterror command.
 *
 *       The RPC is parsed into @error if it is an error and false is
 *       returned.
 *
 *       If the operation was successful, true is returned.
 *
 *       if @gle_doc is not NULL, then the actual response document for
 *       the gle command will be stored as an out parameter. The caller
 *       is responsible for freeing it in this case.
 *
 * Returns:
 *       true if getlasterror was success; otherwise false and @error
 *       is set.
 *
 * Side effects:
 *       @error if return value is false.
 *       @gle_doc will be set if non NULL and a reply was received.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_client_recv_gle (mongoc_client_t  *client,
                         uint32_t          hint,
                         bson_t          **gle_doc,
                         bson_error_t     *error)
{
   mongoc_buffer_t buffer;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bool ret = false;
   bson_t b;

   ENTRY;

   bson_return_val_if_fail (client, false);
   bson_return_val_if_fail (hint, false);

   if (gle_doc) {
      *gle_doc = NULL;
   }

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   if (!_mongoc_cluster_try_recv (&client->cluster, &rpc, &buffer,
                                  hint, error)) {
      GOTO (cleanup);
   }

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Received message other than OP_REPLY.");
      GOTO (cleanup);
   }

   if (_mongoc_rpc_reply_get_first (&rpc.reply, &b)) {
      if (gle_doc) {
         *gle_doc = bson_copy (&b);
      }

      if ((rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
         _bson_to_error (&b, error);
         bson_destroy (&b);
         GOTO (cleanup);
      }

      if (!bson_iter_init_find (&iter, &b, "ok") ||
          BSON_ITER_HOLDS_DOUBLE (&iter)) {
        if (bson_iter_double (&iter) == 0.0) {
          _bson_to_error (&b, error);
        }
      }

      bson_destroy (&b);
   }

   ret = true;

cleanup:
   _mongoc_buffer_destroy (&buffer);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_new --
 *
 *       Create a new mongoc_client_t using the URI provided.
 *
 *       @uri should be a MongoDB URI string such as "mongodb://localhost/"
 *       More information on the format can be found at
 *       http://docs.mongodb.org/manual/reference/connection-string/
 *
 * Returns:
 *       A newly allocated mongoc_client_t or NULL if @uri_string is
 *       invalid.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_client_t *
mongoc_client_new (const char *uri_string)
{
   const mongoc_write_concern_t *write_concern;
   mongoc_client_t *client;
   const bson_t *read_prefs_tags;
   mongoc_uri_t *uri;
   const bson_t *options;
   bson_iter_t iter;
   bool has_ssl = false;
   bool slave_okay = false;

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   options = mongoc_uri_get_options (uri);

   if (bson_iter_init_find (&iter, options, "ssl") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      has_ssl = true;
   }

   if (bson_iter_init_find_case (&iter, options, "slaveok") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      slave_okay = true;
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->request_id = rand ();
   client->initiator = mongoc_client_default_stream_initiator;
   client->initiator_data = client;

   write_concern = mongoc_uri_get_write_concern (uri);
   client->write_concern = mongoc_write_concern_copy (write_concern);

   if (slave_okay) {
      client->read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   } else {
      client->read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   }

   read_prefs_tags = mongoc_uri_get_read_prefs (client->uri);
   if (!bson_empty (read_prefs_tags)) {
      mongoc_read_prefs_set_tags (client->read_prefs, read_prefs_tags);
   }

   _mongoc_cluster_init (&client->cluster, client->uri, client);

#ifdef MONGOC_ENABLE_SSL
   if (has_ssl) {
      mongoc_client_set_ssl_opts (client, mongoc_ssl_opt_get_default ());
   }
#endif

   mongoc_counter_clients_active_inc ();

   return client;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_ssl_opts
 *
 *       set ssl opts for a client
 *
 * Returns:
 *       Nothing
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

#ifdef MONGOC_ENABLE_SSL
void
mongoc_client_set_ssl_opts (mongoc_client_t        *client,
                            const mongoc_ssl_opt_t *opts)
{

   BSON_ASSERT (client);
   BSON_ASSERT (opts);

   memcpy (&client->ssl_opts, opts, sizeof client->ssl_opts);

   bson_free (client->pem_subject);
   client->pem_subject = NULL;

   if (opts->pem_file) {
      client->pem_subject = _mongoc_ssl_extract_subject (opts->pem_file);
   }
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_new_from_uri --
 *
 *       Create a new mongoc_client_t for a mongoc_uri_t.
 *
 * Returns:
 *       A newly allocated mongoc_client_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri)
{
   const char *uristr;

   bson_return_val_if_fail(uri, NULL);

   uristr = mongoc_uri_get_string(uri);
   return mongoc_client_new(uristr);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_destroy --
 *
 *       Destroys a mongoc_client_t and cleans up all resources associated
 *       with the client instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @client is destroyed.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_destroy (mongoc_client_t *client)
{
   if (client) {
#ifdef MONGOC_ENABLE_SSL
      bson_free (client->pem_subject);
#endif

      mongoc_write_concern_destroy (client->write_concern);
      mongoc_read_prefs_destroy (client->read_prefs);
      _mongoc_cluster_destroy (&client->cluster);
      mongoc_uri_destroy (client->uri);
      bson_free (client);

      mongoc_counter_clients_active_dec ();
      mongoc_counter_clients_disposed_inc ();
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_uri --
 *
 *       Fetch the URI used for @client.
 *
 * Returns:
 *       A mongoc_uri_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);

   return client->uri;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_stamp --
 *
 *       INTERNAL API
 *
 *       Fetch the stamp for @node within @client. This is used to track
 *       if there have been changes or disconnects from a node between
 *       the last operation.
 *
 * Returns:
 *       A 32-bit monotonic stamp.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_client_stamp (mongoc_client_t *client,
                      uint32_t    node)
{
   bson_return_val_if_fail (client, 0);

   return _mongoc_cluster_stamp (&client->cluster, node);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_database --
 *
 *       Fetches a newly allocated database structure to communicate with
 *       a database over @client.
 *
 *       @database should be a db name such as "test".
 *
 *       This structure should be freed when the caller is done with it
 *       using mongoc_database_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_database_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
mongoc_client_get_database (mongoc_client_t *client,
                            const char      *name)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   return _mongoc_database_new(client, name, client->read_prefs, client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_collection --
 *
 *       This function returns a newly allocated collection structure.
 *
 *       @db should be the name of the database, such as "test".
 *       @collection should be the name of the collection such as "test".
 *
 *       The above would result in the namespace "test.test".
 *
 *       You should free this structure when you are done with it using
 *       mongoc_collection_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_collection_t *
mongoc_client_get_collection (mongoc_client_t *client,
                              const char      *db,
                              const char      *collection)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);
   bson_return_val_if_fail(collection, NULL);

   return _mongoc_collection_new(client, db, collection, client->read_prefs,
                                 client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_gridfs --
 *
 *       This function returns a newly allocated collection structure.
 *
 *       @db should be the name of the database, such as "test".
 *       @collection should be the name of the collection such as "test".
 *
 *       The above would result in the namespace "test.test".
 *
 *       You should free this structure when you are done with it using
 *       mongoc_collection_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_gridfs_t *
mongoc_client_get_gridfs (mongoc_client_t *client,
                          const char      *db,
                          const char      *prefix,
                          bson_error_t    *error)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);

   if (! prefix) {
      prefix = "fs";
   }

   return _mongoc_gridfs_new(client, db, prefix, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_write_concern --
 *
 *       Fetches the default write concern for @client.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_client_get_write_concern (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);

   return client->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_write_concern --
 *
 *       Sets the default write concern for @client.
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
mongoc_client_set_write_concern (mongoc_client_t              *client,
                                 const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(client);

   if (write_concern != client->write_concern) {
      if (client->write_concern) {
         mongoc_write_concern_destroy(client->write_concern);
      }
      client->write_concern = write_concern ?
         mongoc_write_concern_copy(write_concern) :
         mongoc_write_concern_new();
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_read_prefs --
 *
 *       Fetch the default read preferences for @client.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_client_get_read_prefs (const mongoc_client_t *client)
{
   bson_return_val_if_fail (client, NULL);

   return client->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_read_prefs --
 *
 *       Set the default read preferences for @client.
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
mongoc_client_set_read_prefs (mongoc_client_t           *client,
                              const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail (client);

   if (read_prefs != client->read_prefs) {
      if (client->read_prefs) {
         mongoc_read_prefs_destroy(client->read_prefs);
      }
      client->read_prefs = read_prefs ?
         mongoc_read_prefs_copy(read_prefs) :
         mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
   }
}


bool
_mongoc_client_warm_up (mongoc_client_t *client,
                        bson_error_t    *error)
{
   bool ret = true;
   bson_t cmd;

   BSON_ASSERT (client);

   if (client->cluster.state == MONGOC_CLUSTER_STATE_BORN) {
      bson_init (&cmd);
      bson_append_int32 (&cmd, "ping", 4, 1);
      ret = _mongoc_cluster_command_early (&client->cluster, "admin", &cmd,
                                           NULL, error);
      bson_destroy (&cmd);
   } else if (client->cluster.state == MONGOC_CLUSTER_STATE_DEAD) {
      ret = _mongoc_cluster_reconnect(&client->cluster, error);
   }

   return ret;
}


uint32_t
_mongoc_client_preselect (mongoc_client_t              *client,        /* IN */
                          mongoc_opcode_t               opcode,        /* IN */
                          const mongoc_write_concern_t *write_concern, /* IN */
                          const mongoc_read_prefs_t    *read_prefs,    /* IN */
                          bson_error_t                 *error)         /* OUT */
{

   BSON_ASSERT (client);

   return _mongoc_cluster_preselect (&client->cluster, opcode,
                                     write_concern, read_prefs, error);
}


mongoc_cursor_t *
mongoc_client_command (mongoc_client_t           *client,
                       const char                *db_name,
                       mongoc_query_flags_t       flags,
                       uint32_t                   skip,
                       uint32_t                   limit,
                       uint32_t                   batch_size,
                       const bson_t              *query,
                       const bson_t              *fields,
                       const mongoc_read_prefs_t *read_prefs)
{
   char ns[MONGOC_NAMESPACE_MAX];

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = client->read_prefs;
   }

   /*
    * Allow a caller to provide a fully qualified namespace. Otherwise,
    * querying something like "$cmd.sys.inprog" is not possible.
    */
   if (NULL == strstr (db_name, "$cmd")) {
      bson_snprintf (ns, sizeof ns, "%s.$cmd", db_name);
      db_name = ns;
   }

   return _mongoc_cursor_new (client, db_name, flags, skip, limit, batch_size,
                              true, query, fields, read_prefs);
}


/**
 * mongoc_client_command_simple:
 * @client: A mongoc_client_t.
 * @db_name: The namespace, such as "admin".
 * @command: The command to execute.
 * @read_prefs: The read preferences or NULL.
 * @reply: A location for the reply document or NULL.
 * @error: A location for the error, or NULL.
 *
 * This wrapper around mongoc_client_command() aims to make it simpler to
 * run a command and check the output result.
 *
 * false is returned if the command failed to be delivered or if the execution
 * of the command failed. For example, a command that returns {'ok': 0} will
 * result in this function returning false.
 *
 * To allow the caller to disambiguate between command execution failure and
 * failure to send the command, reply will always be set if non-NULL. The
 * caller should release this with bson_destroy().
 *
 * Returns: true if the command executed and resulted in success. Otherwise
 *   false and @error is set. @reply is always set, either to the resulting
 *   document or an empty bson document upon failure.
 */
bool
mongoc_client_command_simple (mongoc_client_t           *client,
                              const char                *db_name,
                              const bson_t              *command,
                              const mongoc_read_prefs_t *read_prefs,
                              bson_t                    *reply,
                              bson_error_t              *error)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bool ret;

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (command);

   cursor = mongoc_client_command (client, db_name, MONGOC_QUERY_NONE, 0, 1, 0,
                                   command, NULL, read_prefs);

   ret = mongoc_cursor_next (cursor, &doc);

   if (reply) {
      if (ret) {
         bson_copy_to (doc, reply);
      } else {
         bson_init (reply);
      }
   }

   if (!ret) {
      mongoc_cursor_error (cursor, error);
   }

   mongoc_cursor_destroy (cursor);

   return ret;
}

void
mongoc_client_kill_cursor (mongoc_client_t *client,
                           int64_t          cursor_id)
{
   mongoc_rpc_t rpc = {{ 0 }};

   ENTRY;

   bson_return_if_fail(client);
   bson_return_if_fail(cursor_id);

   rpc.kill_cursors.msg_len = 0;
   rpc.kill_cursors.request_id = 0;
   rpc.kill_cursors.response_to = 0;
   rpc.kill_cursors.opcode = MONGOC_OPCODE_KILL_CURSORS;
   rpc.kill_cursors.zero = 0;
   rpc.kill_cursors.cursors = &cursor_id;
   rpc.kill_cursors.n_cursors = 1;

   _mongoc_client_sendv (client, &rpc, 1, 0, NULL, NULL, NULL);

   EXIT;
}


char **
mongoc_client_get_database_names (mongoc_client_t *client,
                                  bson_error_t    *error)
{
   bson_iter_t iter;
   const char *name;
   char **ret = NULL;
   int i = 0;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   BSON_ASSERT (client);

   cursor = mongoc_client_find_databases (client, error);

   while (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init (&iter, doc) &&
          bson_iter_find (&iter, "name") &&
          BSON_ITER_HOLDS_UTF8 (&iter) &&
          (name = bson_iter_utf8 (&iter, NULL)) &&
          (0 != strcmp (name, "local"))) {
            ret = bson_realloc (ret, sizeof(char*) * (i + 2));
            ret [i] = bson_strdup (name);
            ret [++i] = NULL;
         }
   }

   if (!ret) {
      ret = bson_malloc0 (sizeof (void*));
   }

   mongoc_cursor_destroy (cursor);

   return ret;
}


mongoc_cursor_t *
mongoc_client_find_databases (mongoc_client_t *client,
                              bson_error_t    *error)
{
   bson_t cmd = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;

   BSON_ASSERT (client);

   BSON_APPEND_INT32 (&cmd, "listDatabases", 1);

   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_SLAVE_OK, 0, 0, 0,
                                   &cmd, NULL, NULL);

   _mongoc_cursor_array_init(cursor, "databases");

   cursor->limit = 0;

   bson_destroy (&cmd);

   return cursor;
}


int32_t
mongoc_client_get_max_message_size (mongoc_client_t *client) /* IN */
{
   bson_return_val_if_fail (client, -1);

   return client->cluster.max_msg_size;
}


int32_t
mongoc_client_get_max_bson_size (mongoc_client_t *client) /* IN */
{
   bson_return_val_if_fail (client, -1);

   return client->cluster.max_bson_size;
}


bool
mongoc_client_get_server_status (mongoc_client_t     *client,     /* IN */
                                 mongoc_read_prefs_t *read_prefs, /* IN */
                                 bson_t              *reply,      /* OUT */
                                 bson_error_t        *error)      /* OUT */
{
   bson_t cmd = BSON_INITIALIZER;
   bool ret = false;

   bson_return_val_if_fail (client, false);

   BSON_APPEND_INT32 (&cmd, "serverStatus", 1);
   ret = mongoc_client_command_simple (client, "admin", &cmd, read_prefs,
                                       reply, error);
   bson_destroy (&cmd);

   return ret;
}


void
mongoc_client_set_stream_initiator (mongoc_client_t           *client,
                                    mongoc_stream_initiator_t  initiator,
                                    void                      *user_data)
{
   bson_return_if_fail (client);

   if (!initiator) {
      initiator = mongoc_client_default_stream_initiator;
      user_data = client;
   } else {
      MONGOC_DEBUG ("Using custom stream initiator.");
   }

   client->initiator = initiator;
   client->initiator_data = user_data;
}
