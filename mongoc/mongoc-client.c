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
   int s, sfd;

   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

   snprintf(portstr, sizeof portstr, "%hu", host->port);

   memset(&hints, 0, sizeof hints);
   hints.ai_family = host->family;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = 0;
   hints.ai_protocol = 0;

   s = getaddrinfo(host->host, portstr, &hints, &result);
   if (s != 0) {
      bson_set_error(error,
                     MONGOC_ERROR_CONN,
                     MONGOC_ERROR_CONN_NAME_RESOLUTION,
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
                     MONGOC_ERROR_CONN,
                     MONGOC_ERROR_CONN_CONNECT,
                     "Failed to connect to target host.");
      freeaddrinfo(result);
      return NULL;
   }

   freeaddrinfo(result);

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

   sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
   if (sfd == -1) {
      bson_set_error(error,
                     MONGOC_ERROR_CONN,
                     MONGOC_ERROR_CONN_SOCKET,
                     "Failed to create socket.");
      return NULL;
   }

   if (connect(sfd, (struct sockaddr *)&saddr, sizeof saddr) == -1) {
      close(sfd);
      bson_set_error(error,
                     MONGOC_ERROR_CONN,
                     MONGOC_ERROR_CONN_CONNECT,
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
   bson_return_val_if_fail(uri, NULL);
   bson_return_val_if_fail(host, NULL);
   bson_return_val_if_fail(error, NULL);

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
      return mongoc_client_connect_tcp(uri, host, error);
   case AF_UNIX:
      return mongoc_client_connect_unix(uri, host, error);
   default:
      bson_set_error(error,
                     MONGOC_ERROR_CONN,
                     MONGOC_ERROR_CONN_INVALID_TYPE,
                     "Invalid address family");
      return FALSE;
   }
}


static void
mongoc_client_recover (mongoc_client_t *client)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream;
   bson_error_t error = { 0 };

   bson_return_if_fail(client);

   /*
    * Cancel any in flight requests? (Do we need to do this here?)
    * Break all connections.
    * Connect in sequence to each of the seeds (eventually in parallel).
    * Fetch the peer list (eventually need to do Authentication first).
    * Connect to each of the peers.
    */

   if (!(hosts = mongoc_uri_get_hosts(client->uri))) {
      MONGOC_ERROR("No hosts to connect to. Invalid URI.");
      return;
   }

   for (iter = hosts; iter; iter = iter->next) {
      stream = client->initiator(client->uri,
                                 iter,
                                 client->initiator_data,
                                 &error);
      if (!stream) {
         MONGOC_WARNING("Failed to establish stream to %s. Reason: %s",
                        iter->host_and_port, error.message);
         bson_error_destroy(&error);
      }

      MONGOC_DEBUG("Connected to %s", iter->host_and_port);
   }
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
 * Returns: TRUE if successful; otherwise FALSE and @error is set.
 */
bson_bool_t
mongoc_client_send (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_error_t    *error)
{
   mongoc_stream_t *stream;
   bson_bool_t ret;

   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(event, FALSE);

   if (1) mongoc_client_recover(client);

   event->any.opcode = event->type;
   event->any.response_to = -1;
   event->any.request_id = ++client->request_id;

   stream = NULL;
   ret = mongoc_event_write(event, stream, error);

   return ret;
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
   mongoc_uri_destroy(client->uri);
   bson_free(client);
}


const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);
   return client->uri;
}
