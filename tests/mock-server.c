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

#include <bson.h>
#include <errno.h>
#include <fcntl.h>
#include <mongoc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "mock-server.h"
#include "mongoc-buffer-private.h"
#include "mongoc-stream-unix.h"


struct _mock_server_t
{
   mock_server_handler_t  handler;
   void                  *handler_data;
   const char            *address;
   bson_uint16_t          port;
   int                    socket;
};


static void *
mock_server_worker (void *data)
{
   mongoc_buffer_t buffer;
   mongoc_stream_t *stream;
   mock_server_t *server;
   mongoc_rpc_t rpc;
   bson_int32_t msg_len;
   void **closure = data;

   BSON_ASSERT(closure);

   server = closure[0];
   stream = closure[1];

   mongoc_buffer_init(&buffer, NULL, 0, NULL);

again:
   buffer.off = 0;
   if (!mongoc_buffer_append_from_stream(&buffer, stream, 4, 0, NULL)) {
      goto failure;
   }

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if (msg_len < 16) {
      goto failure;
   }

   if (!mongoc_buffer_append_from_stream(&buffer, stream, msg_len - 4, 0, NULL)) {
      goto failure;
   }

   if (!mongoc_rpc_scatter(&rpc, buffer.data, msg_len)) {
      goto failure;
   }

   mongoc_rpc_swab(&rpc);

   server->handler(server, stream, &rpc, server->handler_data);

   goto again;

failure:
   mongoc_stream_close(stream);
   mongoc_stream_destroy(stream);
   bson_free(closure);

   return NULL;
}



mock_server_t *
mock_server_new (const char            *address,
                 bson_uint16_t          port,
                 mock_server_handler_t  handler,
                 void                  *handler_data)
{
   mock_server_t *server;

   bson_return_val_if_fail(handler, NULL);

   if (!address) {
      address = "127.0.0.1";
   }

   if (!port) {
      port = 27017;
   }

   server = bson_malloc0(sizeof *server);
   server->handler = handler;
   server->handler_data = handler_data;
   server->socket = -1;
   server->port = port;

   return server;
}


int
mock_server_run (mock_server_t *server)
{
   struct sockaddr_in saddr;
   mongoc_stream_t *stream;
   pthread_attr_t attr;
   pthread_t thread;
   void **closure;
   int optval;
   int sd;
   int cd;

   bson_return_val_if_fail(server, -1);
   bson_return_val_if_fail(server->socket == -1, -1);

   sd = socket(AF_INET, SOCK_STREAM, 0);
   if (sd == -1) {
      perror("Failed to create socket.");
      return -1;
   }

   optval = 1;
   setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

   memset(&saddr, 0, sizeof saddr);
   memset(&attr, 0, sizeof attr);

   pthread_attr_init(&attr);
   pthread_attr_setstacksize(&attr, 512 * 1024);

   saddr.sin_family = AF_INET;
   saddr.sin_port = htons(server->port);
   /*
    * TODO: Parse server->address.
    */
   saddr.sin_addr.s_addr = htonl(INADDR_ANY);

   if (-1 == bind(sd, (struct sockaddr *)&saddr, sizeof saddr)) {
      perror("Failed to bind socket");
      return -1;
   }

   if (-1 == listen(sd, 10)) {
      perror("Failed to put socket into listen mode");
      return 3;
   }

   server->socket = sd;

   for (;;) {
      cd = accept(server->socket, NULL, NULL);
      if (cd == -1) {
         perror("Failed to accept client socket");
         return -1;
      }

      stream = mongoc_stream_unix_new(cd);
      closure = bson_malloc0(sizeof(void*) * 2);
      closure[0] = server;
      closure[1] = stream;

      pthread_create(&thread, &attr, mock_server_worker, closure);
   }

   close(server->socket);
   server->socket = -1;

   return 0;
}


void
mock_server_quit (mock_server_t *server,
                  int            code)
{
   bson_return_if_fail(server);

   /*
    * TODO: Exit server loop.
    */
}


void
mock_server_destroy (mock_server_t *server)
{
   if (server) {
      bson_free(server);
   }
}
