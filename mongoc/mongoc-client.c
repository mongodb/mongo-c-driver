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
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-collection-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-database-private.h"
#include "mongoc-error.h"
#include "mongoc-list-private.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-queue-private.h"
#include "mongoc-stream-buffered.h"


#ifndef DEFAULT_CONNECTTIMEOUTMS
#define DEFAULT_CONNECTTIMEOUTMS (10 * 1000L)
#endif


struct _mongoc_client_t
{
   bson_uint32_t              request_id;
   mongoc_list_t             *conns;
   mongoc_uri_t              *uri;
   mongoc_cluster_t           cluster;

   mongoc_stream_initiator_t  initiator;
   void                      *initiator_data;

   mongoc_read_prefs_t       *read_prefs;
   mongoc_write_concern_t    *write_concern;
};


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
mongoc_client_connect_tcp (const mongoc_uri_t       *uri,   /* IN */
                           const mongoc_host_list_t *host,  /* IN */
                           bson_error_t             *error) /* OUT */
{
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   bson_uint32_t connecttimeoutms = 0;
   const bson_t *options;
   bson_iter_t iter;
   socklen_t optlen;
   char portstr[8];
   int optval;
   int flags;
   int r;
   int s;
   int sfd;

   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

   options = mongoc_uri_get_options(uri);
   if (bson_iter_init_find(&iter, options, "connecttimeoutms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      connecttimeoutms = bson_iter_int32(&iter);
   }
   if (!connecttimeoutms) {
      connecttimeoutms = DEFAULT_CONNECTTIMEOUTMS;
   }

   snprintf(portstr, sizeof portstr, "%hu", host->port);

   memset(&hints, 0, sizeof hints);
   hints.ai_family = host->family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = 0;
   hints.ai_protocol = 0;

   s = getaddrinfo(host->host, portstr, &hints, &result);
   if (s != 0) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                     "Failed to resolve %s",
                     host->host);
      return NULL;
   }

   for (rp = result; rp; rp = rp->ai_next) {
      sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sfd == -1) {
         continue;
      }

      /*
       * Set the socket non-blocking so we can detect failure to
       * connect by waiting for POLLIN on the socket fd.
       */
      flags = fcntl(sfd, F_GETFL);
      if ((flags & O_NONBLOCK) != O_NONBLOCK) {
         flags = flags | O_NONBLOCK;
         if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) != 0) {
            MONGOC_WARNING("O_NONBLOCK on socket failed. "
                           "Cannot respect connecttimeoutms.");
            if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
               break;
            }
            close(sfd);
            continue;
         }
      }

      r = connect(sfd, rp->ai_addr, rp->ai_addrlen);
      if (r != -1) {
         break;
      }

      if (errno == EINPROGRESS) {
         struct pollfd fds;

again:
         fds.fd = sfd;
         fds.events = POLLOUT;
         fds.revents = 0;
         r = poll(&fds, 1, connecttimeoutms);
         if (r > 0) {
            optval = 0;
            optlen = sizeof optval;
            r = getsockopt(sfd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
            if ((r == -1) || optval != 0) {
               goto cleanup;
            }
            break;
         } else if (r == 0) {
            goto cleanup;
         }

         goto again;
      }

cleanup:
      close(sfd);
   }

   if (!rp) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_CONNECT,
                     "Failed to connect to target host.");
      freeaddrinfo(result);
      return NULL;
   }

   freeaddrinfo(result);

   flags = 1;
   errno = 0;
   r = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flags, sizeof flags);
   if (r < 0) {
      MONGOC_WARNING("Failed to set TCP_NODELAY on fd %u: %s\n",
                     sfd, strerror(errno));
   }

   return mongoc_stream_unix_new(sfd);
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
mongoc_client_connect_unix (const mongoc_uri_t       *uri,   /* IN */
                            const mongoc_host_list_t *host,  /* IN */
                            bson_error_t             *error) /* OUT */
{
   struct sockaddr_un saddr;
   int sfd;

   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

   memset(&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   snprintf(saddr.sun_path, sizeof saddr.sun_path - 1,
            "%s", host->host_and_port);

   sfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sfd == -1) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failed to create socket.");
      return NULL;
   }

   if (connect(sfd, (struct sockaddr *)&saddr, sizeof saddr) == -1) {
      close(sfd);
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_CONNECT,
                     "Failed to connect to UNIX domain socket.");
      return NULL;
   }

   return mongoc_stream_unix_new(sfd);
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
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,       /* IN */
                                        const mongoc_host_list_t *host,      /* IN */
                                        void                     *user_data, /* IN */
                                        bson_error_t             *error)     /* OUT */
{
   mongoc_stream_t *base_stream = NULL;

   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);

   /*
    * TODO:
    *
    *   if ssl option is set, we need to wrap our mongoc_stream_t in
    *   a TLS stream (which needs to be written).
    *
    *   Something like:
    *
    *      mongoc_stream_t *mongoc_stream_new_tls (mongoc_stream_t *)
    */

   switch (host->family) {
   case AF_INET:
      base_stream = mongoc_client_connect_tcp(uri, host, error);
      break;
   case AF_UNIX:
      base_stream = mongoc_client_connect_unix(uri, host, error);
      break;
   default:
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_INVALID_TYPE,
                     "Invalid address family: 0x%02x", host->family);
      break;
   }

   return base_stream ? mongoc_stream_buffered_new(base_stream, 1024) : NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_create_stream --
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
mongoc_client_create_stream (mongoc_client_t          *client, /* IN */
                             const mongoc_host_list_t *host,   /* IN */
                             bson_error_t             *error)  /* OUT */
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

   return client->initiator(client->uri, host, client->initiator_data, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_sendv --
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

bson_uint32_t
mongoc_client_sendv (mongoc_client_t              *client,         /* IN */
                     mongoc_rpc_t                 *rpcs,           /* INOUT */
                     size_t                        rpcs_len,       /* IN */
                     bson_uint32_t                 hint,           /* IN */
                     const mongoc_write_concern_t *write_concern,  /* IN */
                     const mongoc_read_prefs_t    *read_prefs,     /* IN */
                     bson_error_t                 *error)          /* OUT */
{
   size_t i;

   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(rpcs, FALSE);
   bson_return_val_if_fail(rpcs_len, FALSE);

   for (i = 0; i < rpcs_len; i++) {
      rpcs[i].header.msg_len = 0;
      rpcs[i].header.request_id = ++client->request_id;
   }

   switch (client->cluster.state) {
   case MONGOC_CLUSTER_STATE_BORN:
      return mongoc_cluster_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                  write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_HEALTHY:
   case MONGOC_CLUSTER_STATE_UNHEALTHY:
      return mongoc_cluster_try_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                      write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_DEAD:
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "No healthy connections.");
      return FALSE;
   default:
      BSON_ASSERT(FALSE);
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_recv --
 *
 *       Receives a RPC from a remote MongoDB cluster node. @hint should
 *       be the result from a previous call to mongoc_client_sendv() to
 *       signify which node to recv from.
 *
 * Returns:
 *       TRUE if successful; otherwise FALSE and @error is set.
 *
 * Side effects:
 *       @error is set if return value is FALSE.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_client_recv (mongoc_client_t *client, /* IN */
                    mongoc_rpc_t    *rpc,    /* OUT */
                    mongoc_buffer_t *buffer, /* INOUT */
                    bson_uint32_t    hint,   /* IN */
                    bson_error_t    *error)  /* OUT */
{
   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(rpc, FALSE);
   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(hint, FALSE);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, FALSE);

   return mongoc_cluster_try_recv(&client->cluster, rpc, buffer, hint, error);
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
_bson_to_error (const bson_t *b,     /* IN */
                bson_error_t *error) /* OUT */
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
 *       The RPC is parsed into @error if it is an error and FALSE is
 *       returned.
 *
 *       If the operation was successful, TRUE is returned.
 *
 * Returns:
 *       TRUE if getlasterror was success; otherwise FALSE and @error
 *       is set.
 *
 * Side effects:
 *       @error if return value is FALSE.
 *
 *--------------------------------------------------------------------------
 */

bson_bool_t
mongoc_client_recv_gle (mongoc_client_t *client, /* IN */
                        bson_uint32_t    hint,   /* IN */
                        bson_error_t    *error)  /* OUT */
{
   mongoc_buffer_t buffer;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bson_bool_t ret = FALSE;
   bson_t b;

   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(hint, FALSE);
   bson_return_val_if_fail(error, FALSE);

   mongoc_buffer_init(&buffer, NULL, 0, NULL);

   if (!mongoc_cluster_try_recv(&client->cluster, &rpc, &buffer, hint, error)) {
      goto cleanup;
   }

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error(error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Received message other than OP_REPLY.");
      goto cleanup;
   }

   if ((rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if (mongoc_rpc_reply_get_first(&rpc.reply, &b)) {
         _bson_to_error(&b, error);
         bson_destroy(&b);
         goto cleanup;
      }
   }

   if (mongoc_rpc_reply_get_first(&rpc.reply, &b)) {
      if (!bson_iter_init_find(&iter, &b, "ok") ||
          !BSON_ITER_HOLDS_DOUBLE(&iter) ||
          (bson_iter_double(&iter) == 0.0)) {
         _bson_to_error(&b, error);
      }
      bson_destroy(&b);
      goto cleanup;
   }

   ret = TRUE;

cleanup:
   mongoc_buffer_destroy(&buffer);

   return ret;
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
mongoc_client_new (const char *uri_string) /* IN */
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   const bson_t *options;
   bson_iter_t iter;

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   options = mongoc_uri_get_options(uri);
   if (bson_iter_init_find(&iter, options, "ssl") &&
       BSON_ITER_HOLDS_BOOL(&iter) &&
       bson_iter_bool(&iter)) {
      MONGOC_WARNING("SSL is not yet supported!");
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->request_id = rand();
   client->initiator = mongoc_client_default_stream_initiator;
   mongoc_cluster_init(&client->cluster, client->uri, client);

   mongoc_counter_clients_active_inc();

   return client;
}


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
mongoc_client_new_from_uri (const mongoc_uri_t *uri) /* IN */
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
mongoc_client_destroy (mongoc_client_t *client) /* IN */
{
   if (client) {
      /*
       * TODO: Implement destruction.
       */

      mongoc_cluster_destroy(&client->cluster);
      mongoc_uri_destroy(client->uri);
      bson_free(client);

      mongoc_counter_clients_active_dec();
      mongoc_counter_clients_disposed_inc();
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
mongoc_client_get_uri (const mongoc_client_t *client) /* IN */
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

bson_uint32_t
mongoc_client_stamp (mongoc_client_t *client, /* IN */
                     bson_uint32_t    node)   /* IN */
{
   bson_return_val_if_fail(client, 0);
   return mongoc_cluster_stamp(&client->cluster, node);
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
mongoc_client_get_database (mongoc_client_t *client, /* IN */
                            const char      *name)   /* IN */
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   return mongoc_database_new(client, name);
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
mongoc_client_get_collection (mongoc_client_t *client,     /* IN */
                              const char      *db,         /* IN */
                              const char      *collection) /* IN */
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
mongoc_client_get_write_concern (const mongoc_client_t *client) /* IN */
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
mongoc_client_set_write_concern (
      mongoc_client_t              *client,        /* IN */
      const mongoc_write_concern_t *write_concern) /* IN */
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
mongoc_client_get_read_prefs (const mongoc_client_t *client) /* IN */
{
   bson_return_val_if_fail(client, NULL);
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
mongoc_client_set_read_prefs (mongoc_client_t           *client,     /* IN */
                              const mongoc_read_prefs_t *read_prefs) /* IN */
{
   bson_return_if_fail(client);

   if (read_prefs != client->read_prefs) {
      if (client->read_prefs) {
         mongoc_read_prefs_destroy(client->read_prefs);
      }
      client->read_prefs = read_prefs ?
         mongoc_read_prefs_copy(read_prefs) :
         mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
   }
}
