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


#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "mongoc-client.h"
#include "mongoc-client-private.h"
#include "mongoc-cluster-private.h"
#include "mongoc-event-private.h"
#include "mongoc-error.h"
#include "mongoc-list-private.h"
#include "mongoc-log.h"
#include "mongoc-queue-private.h"


struct _mongoc_client_t
{
   bson_uint32_t              request_id;
   mongoc_list_t             *conns;
   mongoc_uri_t              *uri;
   mongoc_cluster_t           cluster;

   mongoc_stream_initiator_t  initiator;
   void                      *initiator_data;
};


static mongoc_stream_t *
mongoc_client_connect_tcp (const mongoc_uri_t       *uri,
                           const mongoc_host_list_t *host,
                           bson_error_t             *error)
{
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   char portstr[8];
   int flag;
   int r;
   int s;
   int sfd;

   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

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

      if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
         break;
      }

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

   flag = 1;
   errno = 0;
   r = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof flag);
   if (r < 0) {
      MONGOC_WARNING("Failed to set TCP_NODELAY on fd %u: %s\n",
                     sfd, strerror(errno));
   }

   return mongoc_stream_new_from_unix(sfd);
}


static mongoc_stream_t *
mongoc_client_connect_unix (const mongoc_uri_t       *uri,
                            const mongoc_host_list_t *host,
                            bson_error_t             *error)
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

   return mongoc_stream_new_from_unix(sfd);
}


static mongoc_stream_t *
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,
                                        const mongoc_host_list_t *host,
                                        void                     *user_data,
                                        bson_error_t             *error)
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

   //return base_stream ? mongoc_stream_buffered_new(base_stream) : NULL;
   return base_stream;
}


mongoc_stream_t *
mongoc_client_create_stream (mongoc_client_t          *client,
                             const mongoc_host_list_t *host,
                             bson_error_t             *error)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

   return client->initiator(client->uri, host, client->initiator_data, error);
}


void
mongoc_client_prepare_event (mongoc_client_t *client,
                             mongoc_event_t  *event)
{
   bson_return_if_fail(client);
   bson_return_if_fail(event);

   event->any.opcode = event->type;
   event->any.response_to = -1;
   event->any.request_id = ++client->request_id;
}


/**
 * mongoc_client_send:
 * @client: (in): A mongoc_client_t.
 * @event: (in) (transfer full): A mongoc_event_t.
 * @error: (out): A location for a bson_error_t or NULL.
 *
 * Send an event via @client to the MongoDB server. The event structure
 * is mutated by @client in the process and therefore should be considered
 * destroyed after calling this function. No further access to @event should
 * occur after calling this method.
 *
 * The return value contains a hint for the cluster node that was used.
 * You can provide this value on a suplimental call to reselect the same
 * cluster node for communication.
 *
 * Returns: Greater than 0 if successful; otherwise 0 and @error is set.
 */
bson_uint32_t
mongoc_client_send (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_uint32_t    hint,
                    bson_error_t    *error)
{
   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(event, FALSE);

   switch (client->cluster.state) {
   case MONGOC_CLUSTER_STATE_BORN:
      return mongoc_cluster_send(&client->cluster, event, hint, error);
   case MONGOC_CLUSTER_STATE_HEALTHY:
   case MONGOC_CLUSTER_STATE_UNHEALTHY:
      return mongoc_cluster_try_send(&client->cluster, event, hint, error);
   case MONGOC_CLUSTER_STATE_DEAD:
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "No healthy connections.");
      return FALSE;
   default:
      assert(FALSE);
      break;
   }
}


bson_bool_t
mongoc_client_recv (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_uint32_t    hint,
                    bson_error_t    *error)
{
   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(hint, FALSE);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, FALSE);

   return mongoc_cluster_try_recv(&client->cluster, event, hint, error);
}


mongoc_client_t *
mongoc_client_new (const char *uri_string)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->request_id = rand();
   client->initiator = mongoc_client_default_stream_initiator;
   mongoc_cluster_init(&client->cluster, client->uri, client);

   return client;
}


mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri)
{
   const char *uristr;

   bson_return_val_if_fail(uri, NULL);

   uristr = mongoc_uri_get_string(uri);
   return mongoc_client_new(uristr);
}


void
mongoc_client_destroy (mongoc_client_t *client)
{
   /*
    * TODO: Implement destruction.
    */
   mongoc_cluster_destroy(&client->cluster);
   mongoc_uri_destroy(client->uri);
   bson_free(client);
}


const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);
   return client->uri;
}
