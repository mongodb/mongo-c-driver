/*
 * Copyright 2015 MongoDB, Inc.
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


#include "mongoc-rpc-private.h"
#include "mongoc-opcode.h"
#include "mongoc-flags.h"
#include "mongoc-buffer-private.h"
#include "mongoc-stream-socket.h"
#include "mongoc-thread-private.h"
#include "mongoc-trace.h"
#include "queue.h"
#include "mock-server2.h"


#ifdef MONGOC_ENABLE_SSL

#include "mongoc-stream-tls.h"
#include "future.h"

#endif

#ifdef _WIN32
# define strcasecmp _stricmp
#endif


#define TIMEOUT 100


struct _mock_server2_t
{
   bool running;
   bool stopped;
   bool verbose;
   uint16_t port;
   mongoc_socket_t *sock;
   char *uri_str;
   mongoc_uri_t *uri;
   mongoc_thread_t main_thread;
   mongoc_cond_t cond;
   mongoc_mutex_t mutex;
   int32_t last_response_id;
   mongoc_array_t worker_threads;
   queue_t *q;

   bool isMaster;
   int minWireVersion;
   int maxWireVersion;
   int maxBsonObjectSize;
   int maxMessageSizeBytes;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t *ssl_opts;
#endif
};


struct _request_t
{
   const mongoc_rpc_t *request_rpc;
   mock_server2_t *server;
   mongoc_stream_t *client;
};


static void *main_thread (void *data);

static void *worker_thread (void *data);

static bool handle_command (mock_server2_t *server,
                            mongoc_stream_t *client,
                            mongoc_rpc_t *rpc);

static bool handle_ismaster (mock_server2_t *server,
                             mongoc_stream_t *client,
                             mongoc_rpc_t *rpc,
                             const bson_t *doc);

static void mock_server2_reply_simple (mock_server2_t *server,
                                       mongoc_stream_t *client,
                                       const mongoc_rpc_t *request,
                                       mongoc_reply_flags_t flags,
                                       const bson_t *doc);

static char *
_single_quotes_to_double (const char *str);


mock_server2_t *
mock_server2_new ()
{
   mock_server2_t *server = bson_malloc0 (sizeof (mock_server2_t));

   _mongoc_array_init (&server->worker_threads, sizeof (mongoc_thread_t));
   mongoc_cond_init (&server->cond);
   mongoc_mutex_init (&server->mutex);
   server->q = q_new ();

   /* TODO configurable, and auto-ismaster defaults "off" */
   server->isMaster = true;
   server->maxWireVersion = 3;
   server->maxBsonObjectSize = 16 * 1024 * 1024;
   server->maxMessageSizeBytes = 32 * 1024 * 1024;

   return server;
}


uint16_t
get_port (mongoc_socket_t *sock)
{
   struct sockaddr_in bound_addr = { 0 };
   socklen_t addr_len;

   addr_len = (socklen_t) sizeof bound_addr;
   if (mongoc_socket_getsockname (sock,
                                  (struct sockaddr *) &bound_addr,
                                  &addr_len) < 0) {
      perror ("Failed to get listening port number");
      return 0;
   }

   return ntohs (bound_addr.sin_port);
}


uint16_t
mock_server2_run (mock_server2_t *server)
{
   mongoc_socket_t *ssock;
   struct sockaddr_in bind_addr;
   int optval;
   uint16_t bound_port;

   MONGOC_INFO ("Starting mock server on port %d.", server->port);

   ssock = mongoc_socket_new (AF_INET, SOCK_STREAM, 0);
   if (!ssock) {
      perror ("Failed to create socket.");
      return 0;
   }

   optval = 1;
   mongoc_socket_setsockopt (ssock, SOL_SOCKET, SO_REUSEADDR, &optval,
                             sizeof optval);

   memset (&bind_addr, 0, sizeof bind_addr);

   bind_addr.sin_family = AF_INET;
   bind_addr.sin_addr.s_addr = htonl (INADDR_ANY);

   /* bind to unused port */
   bind_addr.sin_port = htons (0);

   if (-1 == mongoc_socket_bind (ssock,
                                 (struct sockaddr *) &bind_addr,
                                 sizeof bind_addr)) {
      perror ("Failed to bind socket");
      return 0;
   }

   if (-1 == mongoc_socket_listen (ssock, 10)) {
      perror ("Failed to put socket into listen mode");
      return 0;
   }

   bound_port = get_port (ssock);
   if (!bound_port) {
      perror ("Failed to get bound port number");
      return 0;
   }

   mongoc_mutex_lock (&server->mutex);
   server->sock = ssock;
   server->port = bound_port;
   /* TODO: configurable socket timeout, perhaps from env */
   server->uri_str = bson_strdup_printf (
         "mongodb://127.0.0.1:%hu/?serverselectiontimeoutms=10000000",
         bound_port);
   server->uri = mongoc_uri_new (server->uri_str);
   mongoc_mutex_unlock (&server->mutex);

   mongoc_thread_create (&server->main_thread, main_thread, (void *) server);

   return (uint16_t) bound_port;
}


const mongoc_uri_t *
mock_server2_get_uri (mock_server2_t *server)
{
   mongoc_uri_t *uri;

   mongoc_mutex_lock (&server->mutex);
   uri = server->uri;
   mongoc_mutex_unlock (&server->mutex);

   return uri;
}


bool
mock_server2_get_verbose (mock_server2_t *server)
{
   bool verbose;

   mongoc_mutex_lock (&server->mutex);
   verbose = server->verbose;
   mongoc_mutex_unlock (&server->mutex);

   return verbose;
}

void
mock_server2_set_verbose (mock_server2_t *server, bool verbose)
{
   mongoc_mutex_lock (&server->mutex);
   server->verbose = verbose;
   mongoc_mutex_unlock (&server->mutex);
}


queue_t *
mock_server2_get_queue (mock_server2_t *server)
{
   queue_t *q;

   mongoc_mutex_lock (&server->mutex);
   q = server->q;
   mongoc_mutex_unlock (&server->mutex);

   return q;
}


request_t *
mock_server2_receives_command (mock_server2_t *server,
                               const char *database_name,
                               const char *command_name,
                               const char *command_json)
{
   queue_t *q;
   request_t *request;

   q = mock_server2_get_queue (server);
   /* TODO: get timeout val from mock_server2_t */
   request = (request_t *) q_get (q, 100 * 1000);

   return request;
}


void
mock_server2_replies (request_t *request,
                      uint32_t flags,
                      int64_t cursor_id,
                      int32_t starting_from,
                      int32_t number_returned,
                      const char *docs_json)
{
   char *quotes_replaced = _single_quotes_to_double (docs_json);
   bson_t doc;
   bson_error_t error;
   bool r;


   r = bson_init_from_json (&doc, quotes_replaced, -1, &error);
   if (!r) {
      MONGOC_WARNING ("%s", error.message);
      return;
   }

   mock_server2_reply_simple (request->server,
                              request->client,
                              request->request_rpc,
                              MONGOC_REPLY_NONE,
                              &doc);

   bson_destroy (&doc);
   bson_free (quotes_replaced);
}


void
mock_server2_destroy (mock_server2_t *server)
{
   int64_t deadline = bson_get_monotonic_time () + 10 * 1000 * 1000;

   /* TODO: improve! */
   mongoc_mutex_lock (&server->mutex);
   if (server->running) {
      server->stopped = true;
   }
   mongoc_mutex_unlock (&server->mutex);

   while (bson_get_monotonic_time () <= deadline) {
      /* wait 10 seconds */
      mongoc_mutex_lock (&server->mutex);
      if (!server->running) {
         mongoc_mutex_unlock (&server->mutex);
         break;
      }

      mongoc_mutex_unlock (&server->mutex);
      usleep (1000);
   }

   mongoc_mutex_lock (&server->mutex);
   if (server->running) {
      fprintf (stderr, "server still running after timeout\n");
      abort ();
   }

   _mongoc_array_destroy (&server->worker_threads);
   mongoc_cond_destroy (&server->cond);
   mongoc_mutex_unlock (&server->mutex);
   mongoc_mutex_destroy (&server->mutex);
   q_destroy (server->q);
   bson_free (server);
}


request_t *
request_new (const mongoc_rpc_t *request_rpc,
             mock_server2_t *server,
             mongoc_stream_t *client)
{
   request_t *request;

   request = bson_malloc (sizeof *request);
   request->request_rpc = request_rpc;
   request->server = server;
   request->client = client;

   return request;
}


void
request_destroy (request_t *request)
{
   /* TODO */
}


static void *
background_bulk_operation_execute (void *data)
{
   future_t *future = (future_t *) data;
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   future_value_set_uint32_t (
         &return_value,
         mongoc_bulk_operation_execute (
               future_value_get_mongoc_bulk_operation_ptr (&copy->argv[0]),
               future_value_get_bson_ptr (&copy->argv[1]),
               future_value_get_bson_error_ptr (&copy->argv[2])));

   future_resolve (future, return_value);

   return NULL;
}


future_t *
future_bulk_operation_execute (mongoc_bulk_operation_t *bulk,
                               bson_t *reply,
                               bson_error_t *error)
{
   future_t *future;

   future = future_new (3);

   /* TODO: use setters */
   future->return_value.type = future_value_uint32_t_type;

   future->argv[0].type = future_value_mongoc_bulk_operation_ptr_type;
   future->argv[0].mongoc_bulk_operation_ptr_value = bulk;

   future->argv[1].type = future_value_bson_ptr_type;
   future->argv[1].bson_ptr_value = reply;

   future->argv[2].type = future_value_bson_error_ptr_type;
   future->argv[2].bson_error_ptr_value = error;

   future_start (future, background_bulk_operation_execute);

   return future;
}


typedef struct worker_closure_t
{
   mock_server2_t *server;
   mongoc_stream_t *client_stream;
   uint16_t port;
} worker_closure_t;


static void *
main_thread (void *data)
{
   mock_server2_t *server = data;
   mongoc_socket_t *client_sock;
   bool stopped;
   uint16_t port;
   mongoc_stream_t *client_stream;
   worker_closure_t *closure;
   mongoc_thread_t thread;

   mongoc_mutex_lock (&server->mutex);
   server->running = true;
   mongoc_cond_signal (&server->cond);
   mongoc_mutex_unlock (&server->mutex);

   for (;;) {
      client_sock = mongoc_socket_accept (server->sock,
                                          bson_get_monotonic_time () + TIMEOUT);

      mongoc_mutex_lock (&server->mutex);
      stopped = server->stopped;
      mongoc_mutex_unlock (&server->mutex);

      if (stopped) {
         break;
      }

      if (client_sock) {
         port = get_port (client_sock);
         if (mock_server2_get_verbose (server)) {
            printf ("connection from port %hu\n", port);
         }

         client_stream = mongoc_stream_socket_new (client_sock);

#ifdef MONGOC_ENABLE_SSL
         if (server->ssl_opts) {
            client_stream = mongoc_stream_tls_new (client_stream,
                                                   server->ssl_opts, 0);
            if (!client_stream) {
               perror ("Failed to attach tls stream");
               break;
            }
         }
#endif
         closure = bson_malloc (sizeof *closure);
         closure->server = server;
         closure->client_stream = client_stream;
         closure->port = port;

         mongoc_mutex_lock (&server->mutex);
         /* TODO: add to array of threads */
         mongoc_mutex_unlock (&server->mutex);

         mongoc_thread_create (&thread, worker_thread, closure);
      }
   }

   /* TODO: cleanup */

   mongoc_mutex_lock (&server->mutex);
   server->running = false;
   mongoc_mutex_unlock (&server->mutex);

   return NULL;
}

static void *
worker_thread (void *data)
{
   worker_closure_t *closure = (worker_closure_t *) data;
   mock_server2_t *server = closure->server;
   mongoc_stream_t *client_stream = closure->client_stream;
   uint16_t port = closure->port;
   mongoc_buffer_t buffer;
   mongoc_rpc_t *rpc;
   bson_error_t error;
   int32_t msg_len;
   bool stopped;
   queue_t *q;
   request_t *request;

   ENTRY;

   BSON_ASSERT(closure);

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

again:
   mongoc_mutex_lock (&server->mutex);
   stopped = server->stopped;
   mongoc_mutex_unlock (&server->mutex);

   if (stopped) {
      goto failure;
   }

   if (_mongoc_buffer_fill (&buffer, client_stream, 4, TIMEOUT, &error) == -1) {
      GOTO (again);
   }

   assert (buffer.len >= 4);

   memcpy (&msg_len, buffer.data + buffer.off, 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);

   if (msg_len < 16) {
      MONGOC_WARNING ("No data");
      GOTO (failure);
   }

   if (_mongoc_buffer_fill (&buffer, client_stream, (size_t) msg_len, -1,
                            &error) == -1) {
      MONGOC_WARNING ("%s():%d: %s", __FUNCTION__, __LINE__, error.message);
      GOTO (failure);
   }

   assert (buffer.len >= (unsigned) msg_len);

   DUMP_BYTES (buffer, buffer.data + buffer.off, buffer.len);

   rpc = bson_malloc0 (sizeof *rpc);
   if (!_mongoc_rpc_scatter (rpc, buffer.data + buffer.off,
                             (size_t) msg_len)) {
      MONGOC_WARNING ("%s():%d: %s", __FUNCTION__, __LINE__,
                      "Failed to scatter");
      GOTO (failure);
   }

   _mongoc_rpc_swab_from_le (rpc);

   if (!handle_command (server, client_stream, rpc)) {
      if (mock_server2_get_verbose (server)) {
         /* TODO: parse and print repr of request */
         /*printf ("%hu\t%s\n", port, "unhandled command");*/
      }

      q = mock_server2_get_queue (server);
      request = request_new (rpc, server, client_stream);
      q_put (q, (void *)request);
   }

   memmove (buffer.data, buffer.data + buffer.off + msg_len,
            buffer.len - msg_len);
   buffer.off = 0;
   buffer.len -= msg_len;

   GOTO (again);

failure:
   mongoc_mutex_lock (&server->mutex);
   /* TODO: remove self from threads array */
   mongoc_mutex_unlock (&server->mutex);

   _mongoc_buffer_destroy (&buffer);

   mongoc_stream_close (client_stream);
   mongoc_stream_destroy (client_stream);
   bson_free (closure);
   _mongoc_buffer_destroy (&buffer);

   RETURN (NULL);
}


static bool
handle_command (mock_server2_t *server,
                mongoc_stream_t *client,
                mongoc_rpc_t *rpc)
{
   int32_t len;
   bool ret = false;
   bson_iter_t iter;
   const char *key;
   bson_t doc;

   BSON_ASSERT (rpc);

   if (rpc->header.opcode != MONGOC_OPCODE_QUERY) {
      return false;
   }

   memcpy (&len, rpc->query.query, 4);
   len = BSON_UINT32_FROM_LE (len);
   if (!bson_init_static (&doc, rpc->query.query, len)) {
      return false;
   }

   if (!bson_iter_init (&iter, &doc) || !bson_iter_next (&iter)) {
      return false;
   }

   key = bson_iter_key (&iter);

   if (!strcasecmp (key, "ismaster")) {
      ret = handle_ismaster (server, client, rpc, &doc);
   }

   bson_destroy (&doc);

   return ret;
}


static bool
handle_ismaster (mock_server2_t *server,
                 mongoc_stream_t *client,
                 mongoc_rpc_t *rpc,
                 const bson_t *doc)
{
   bson_t reply_doc = BSON_INITIALIZER;
   time_t now = time (NULL);

   BSON_ASSERT (server);
   BSON_ASSERT (client);
   BSON_ASSERT (rpc);
   BSON_ASSERT (doc);

   bson_append_bool (&reply_doc, "ismaster", -1, server->isMaster);
   bson_append_int32 (&reply_doc, "maxBsonObjectSize", -1,
                      server->maxBsonObjectSize);
   bson_append_int32 (&reply_doc, "maxMessageSizeBytes", -1,
                      server->maxMessageSizeBytes);
   bson_append_int32 (&reply_doc, "minWireVersion", -1,
                      server->minWireVersion);
   bson_append_int32 (&reply_doc, "maxWireVersion", -1,
                      server->maxWireVersion);
   bson_append_double (&reply_doc, "ok", -1, 1.0);
   bson_append_time_t (&reply_doc, "localtime", -1, now);

   mock_server2_reply_simple (server, client, rpc, MONGOC_REPLY_NONE,
                              &reply_doc);

   bson_destroy (&reply_doc);

   return true;
}


void
mock_server2_reply_simple (mock_server2_t *server,
                           mongoc_stream_t *client,
                           const mongoc_rpc_t *request,
                           mongoc_reply_flags_t flags,
                           const bson_t *doc)
{
   mongoc_iovec_t *iov;
   mongoc_array_t ar;
   mongoc_rpc_t r = {{ 0 }};
   size_t expected = 0;
   ssize_t n_written;
   int iovcnt;
   int i;

   BSON_ASSERT (server);
   BSON_ASSERT (request);
   BSON_ASSERT (client);
   BSON_ASSERT (doc);

   _mongoc_array_init (&ar, sizeof (mongoc_iovec_t));

   mongoc_mutex_lock (&server->mutex);
   r.reply.request_id = ++server->last_response_id;
   mongoc_mutex_unlock (&server->mutex);
   r.reply.msg_len = 0;
   r.reply.response_to = request->header.request_id;
   r.reply.opcode = MONGOC_OPCODE_REPLY;
   r.reply.flags = flags;
   r.reply.cursor_id = 0;
   r.reply.start_from = 0;
   r.reply.n_returned = 1;
   r.reply.documents = bson_get_data (doc);
   r.reply.documents_len = doc->len;

   _mongoc_rpc_gather (&r, &ar);
   _mongoc_rpc_swab_to_le (&r);

   iov = ar.data;
   iovcnt = (int) ar.len;

   for (i = 0; i < iovcnt; i++) {
      expected += iov[i].iov_len;
   }

   n_written = mongoc_stream_writev (client, iov, iovcnt, -1);

   assert (n_written == expected);

   _mongoc_array_destroy (&ar);
}

/* TODO: refactor with test-bulk */
/* copy str with single-quotes replaced by double. bson_free the return value.*/
static char *
_single_quotes_to_double (const char *str)
{
   char *result = bson_strdup (str);
   char *p;

   for (p = result; *p; p++) {
      if (*p == '\'') {
         *p = '"';
      }
   }

   return result;
}
