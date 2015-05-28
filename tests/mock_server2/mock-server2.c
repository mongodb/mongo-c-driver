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


#include <mongoc-rpc-private.h>
#include "mongoc.h"

#include "mongoc-rpc-private.h"
#include "mongoc-buffer-private.h"
#include "mongoc-thread-private.h"
#include "mongoc-trace.h"
#include "sync-queue.h"
#include "mock-server2.h"
#include "../test-conveniences.h"


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
   sync_queue_t *q;
   mongoc_array_t autoresponders;
   int last_autoresponder_id;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t *ssl_opts;
#endif
};


struct _request_t
{
   mongoc_rpc_t     request_rpc;
   mock_server2_t  *server;
   mongoc_stream_t *client;
   bool             is_command;
   char            *command_name;
   mongoc_array_t   docs;
};


struct _autoresponder_handle_t
{
   autoresponder_t responder;
   void           *data;
   destructor_t    destructor;
   int             id;
};


static void *main_thread (void *data);

static void *worker_thread (void *data);

static void mock_server2_reply_simple (mock_server2_t *server,
                                       mongoc_stream_t *client,
                                       const mongoc_rpc_t *request,
                                       mongoc_reply_flags_t flags,
                                       const bson_t *doc);

bool
request_matches_query (const request_t     *request,
                       const char          *ns,
                       mongoc_query_flags_t flags,
                       uint32_t             skip,
                       uint32_t             n_return,
                       const char          *query_json,
                       const char          *fields_json);

void autoresponder_handle_destroy (autoresponder_handle_t *handle);


/*--------------------------------------------------------------------------
 *
 * mock_server2_new --
 *
 *       Get a new mock_server2_t. Call mock_server2_run to start it,
 *       then mock_server2_get_uri to connect.
 *
 *       This server does not autorespond to "ismaster".
 *
 * Returns:
 *       A server you must mock_server2_destroy.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mock_server2_t *
mock_server2_new ()
{
   mock_server2_t *server = bson_malloc0 (sizeof (mock_server2_t));

   _mongoc_array_init (&server->autoresponders,
                       sizeof (autoresponder_handle_t));
   _mongoc_array_init (&server->worker_threads,
                       sizeof (mongoc_thread_t));
   mongoc_cond_init (&server->cond);
   mongoc_mutex_init (&server->mutex);
   server->q = q_new ();

   return server;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_with_autoismaster --
 *
 *       A new mock_server2_t that autoresponds to ismaster. Call
 *       mock_server2_run to start it, then mock_server2_get_uri to
 *       connect.
 *
 * Returns:
 *       A server you must mock_server2_destroy.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mock_server2_t *
mock_server2_with_autoismaster (int32_t max_wire_version)
{
   mock_server2_t *server = mock_server2_new ();
   char *ismaster = bson_strdup_printf ("{'ismaster': true,"
                                        " 'minWireVersion': 0,"
                                        " 'maxWireVersion': %d}",
                                        max_wire_version);

   mock_server2_auto_ismaster (server, ismaster);

   bson_free (ismaster);

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


/*--------------------------------------------------------------------------
 *
 * mock_server2_run --
 *
 *       Start listening on an unused port. After this, call
 *       mock_server2_get_uri to connect.
 *
 * Returns:
 *       The bound port.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

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
   /* TODO: configurable timeouts, perhaps from env */
   server->uri_str = bson_strdup_printf (
         "mongodb://127.0.0.1:%hu/?serverselectiontimeoutms=10000&"
         "sockettimeoutms=10000",
         bound_port);
   server->uri = mongoc_uri_new (server->uri_str);
   mongoc_mutex_unlock (&server->mutex);

   mongoc_thread_create (&server->main_thread, main_thread, (void *) server);

   return (uint16_t) bound_port;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_autoresponds --
 *
 *       Respond to matching requests. "data" is passed to the responder
 *       callback, and passed to "destructor" when the autoresponder is
 *       destroyed.
 *
 *       Responders are run most-recently-added-first until one returns
 *       true to indicate it has handled the request. If none handles it,
 *       the request is enqueued until a call to mock_server2_receives_*.
 *
 * Returns:
 *       An id for mock_server2_remove_autoresponder.
 *
 * Side effects:
 *       If a matching request is enqueued, pop it and respond.
 *
 *--------------------------------------------------------------------------
 */

int
mock_server2_autoresponds (mock_server2_t *server,
                           autoresponder_t responder,
                           void           *data,
                           destructor_t    destructor)
{
   autoresponder_handle_t handle = { responder, data, destructor };
   int id;

   mongoc_mutex_lock (&server->mutex);
   id = handle.id = server->last_autoresponder_id++;
   /* TODO: peek and see if a matching request is enqueued */
   _mongoc_array_append_val (&server->autoresponders, handle);
   mongoc_mutex_unlock (&server->mutex);

   return id;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_remove_autoresponder --
 *
 *       Remove a responder callback. Pass in the id returned by
 *       mock_server2_autoresponds.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       The responder's destructor is called on its "data" pointer.
 *
 *--------------------------------------------------------------------------
 */

void
mock_server2_remove_autoresponder (mock_server2_t *server,
                                   int id)
{
   size_t i;
   autoresponder_handle_t *handles;
   
   mongoc_mutex_lock (&server->mutex);
   handles = (autoresponder_handle_t *)server->autoresponders.data;
   for (i = 0; i < server->autoresponders.len; i++) {
      if (handles[i].id == id) {
         /* left-shift everyone after */
         server->autoresponders.len--;
         for (; i < server->autoresponders.len; i++) {
            handles[i] = handles[i + 1];
         }

         autoresponder_handle_destroy (handles);

         break;
      }
   }

   mongoc_mutex_unlock (&server->mutex);
}


static bool
auto_ismaster (request_t *request,
               void      *data)
{
   const char *response_json = (const char*)data;
   char *quotes_replaced;
   bson_t response;
   bson_error_t error;

   if (!request->is_command || strcasecmp (request->command_name, "ismaster")) {
      return false;
   }

   quotes_replaced = single_quotes_to_double (response_json);

   if (!bson_init_from_json (&response, quotes_replaced, -1, &error)) {
      fprintf (stderr, "%s\n", error.message);
      abort ();
   }

   mock_server2_reply_simple (request->server,
                              request->client,
                              &request->request_rpc,
                              MONGOC_REPLY_NONE,
                              &response);

   bson_destroy (&response);
   bson_free (quotes_replaced);
   return true;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_auto_ismaster --
 *
 *       Autorespond to "ismaster" with the provided document.
 *
 * Returns:
 *       An id for mock_server2_remove_autoresponder.
 *
 * Side effects:
 *       If a matching request is enqueued, pop it and respond.
 *
 *--------------------------------------------------------------------------
 */

int
mock_server2_auto_ismaster (mock_server2_t *server,
                            const char *response_json)
{
   char *copy = bson_strdup (response_json);

   return mock_server2_autoresponds (server,
                                     auto_ismaster,
                                     (void *)copy,
                                     bson_free);
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_get_uri --
 *
 *       Call after mock_server2_run to get the connection string.
 *
 * Returns:
 *       A const string.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_uri_t *
mock_server2_get_uri (mock_server2_t *server)
{
   mongoc_uri_t *uri;

   mongoc_mutex_lock (&server->mutex);
   uri = server->uri;
   mongoc_mutex_unlock (&server->mutex);

   return uri;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_get_verbose --
 *
 *       Is the server set to log during normal operation?
 *
 *--------------------------------------------------------------------------
 */

bool
mock_server2_get_verbose (mock_server2_t *server)
{
   bool verbose;

   mongoc_mutex_lock (&server->mutex);
   verbose = server->verbose;
   mongoc_mutex_unlock (&server->mutex);

   return verbose;
}

/*--------------------------------------------------------------------------
 *
 * mock_server2_set_verbose --
 *
 *       Tell the server whether to log during normal operation.
 *
 *--------------------------------------------------------------------------
 */

void
mock_server2_set_verbose (mock_server2_t *server, bool verbose)
{
   mongoc_mutex_lock (&server->mutex);
   server->verbose = verbose;
   mongoc_mutex_unlock (&server->mutex);
}


sync_queue_t *
mock_server2_get_queue (mock_server2_t *server)
{
   sync_queue_t *q;

   mongoc_mutex_lock (&server->mutex);
   q = server->q;
   mongoc_mutex_unlock (&server->mutex);

   return q;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_receives_command --
 *
 *       Pop a client request if one is enqueued, or wait up to
 *       request_timeout_ms for the client to send a request.
 *
 * Returns:
 *       A request you must request_destroy.
 *
 * Side effects:
 *       Logs and aborts if the current request is not a command matching
 *       database_name, command_name, and command_json.
 *
 *--------------------------------------------------------------------------
 */

request_t *
mock_server2_receives_command (mock_server2_t *server,
                               const char     *database_name,
                               const char     *command_json)
{
   sync_queue_t *q;
   request_t *request;

   q = mock_server2_get_queue (server);
   /* TODO: get timeout val from mock_server2_t */
   request = (request_t *) q_get (q, 100 * 1000);

   /* TODO: match */

   return request;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_receives_query --
 *
 *       Pop a client request if one is enqueued, or wait up to
 *       request_timeout_ms for the client to send a request.
 *
 * Returns:
 *       A request you must request_destroy.
 *
 * Side effects:
 *       Logs and aborts if the current request is not a query matching
 *       ns, flags, skip, n_return, query_json, and fields_json.
 *
 *--------------------------------------------------------------------------
 */

request_t *
mock_server2_receives_query (mock_server2_t      *server,
                             const char          *ns,
                             mongoc_query_flags_t flags,
                             uint32_t             skip,
                             uint32_t             n_return,
                             const char          *query_json,
                             const char          *fields_json)
{
   sync_queue_t *q;
   request_t *request;

   q = mock_server2_get_queue (server);
   /* TODO: get timeout val from mock_server2_t */
   request = (request_t *)q_get (q, 100 * 1000);

   if (!request_matches_query (request,
                               ns,
                               flags,
                               skip,
                               n_return,
                               query_json,
                               fields_json)) {
      request_destroy (request);
      return NULL;
   }

   return request;
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_hangs_up --
 *
 *       Hang up on a client request.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Causes a network error on the client side.
 *
 *--------------------------------------------------------------------------
 */

void
mock_server2_hangs_up (request_t *request)
{
   mongoc_stream_close (request->client);
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_replies --
 *
 *       Respond to a client request.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Sends an OP_REPLY to the client.
 *
 *--------------------------------------------------------------------------
 */

void
mock_server2_replies (request_t  *request,
                      uint32_t    flags,
                      int64_t     cursor_id,
                      int32_t     starting_from,
                      int32_t     number_returned,
                      const char *docs_json)
{
   char *quotes_replaced = single_quotes_to_double (docs_json);
   bson_t doc;
   bson_error_t error;
   bool r;

   assert (request);

   r = bson_init_from_json (&doc, quotes_replaced, -1, &error);
   if (!r) {
      MONGOC_WARNING ("%s", error.message);
      return;
   }

   mock_server2_reply_simple (request->server,
                              request->client,
                              &request->request_rpc,
                              MONGOC_REPLY_NONE,
                              &doc);

   bson_destroy (&doc);
   bson_free (quotes_replaced);
}


/*--------------------------------------------------------------------------
 *
 * mock_server2_destroy --
 *
 *       Free a mock_server2_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Closes sockets, joins threads, and calls destructors passed
 *       to mock_server2_autoresponds.
 *
 *--------------------------------------------------------------------------
 */


void
mock_server2_destroy (mock_server2_t *server)
{
   size_t i;
   autoresponder_handle_t *handle;
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

   for (i = 0; i < server->autoresponders.len; i++) {
      handle = &_mongoc_array_index (&server->autoresponders,
                                     autoresponder_handle_t,
                                     i);

      autoresponder_handle_destroy (handle);
   }

   _mongoc_array_destroy (&server->autoresponders);

   mongoc_cond_destroy (&server->cond);
   mongoc_mutex_unlock (&server->mutex);
   mongoc_mutex_destroy (&server->mutex);
   mongoc_socket_destroy (server->sock);
   bson_free (server->uri_str);
   mongoc_uri_destroy (server->uri);
   q_destroy (server->q);
   bson_free (server);
}


bool
is_command (const char *ns)
{
   size_t len = strlen (ns);
   const char *cmd = ".$cmd";
   size_t cmd_len = strlen (cmd);

   return len > cmd_len && !strncmp (ns + len - cmd_len, cmd, cmd_len);
}


request_t *
request_new (const mongoc_rpc_t *request_rpc,
             mock_server2_t *server,
             mongoc_stream_t *client)
{
   request_t *request;
   int32_t len;
   bson_t *query;
   bson_iter_t iter;

   request = bson_malloc0 (sizeof *request);
   memcpy ((void *)&request->request_rpc,
           (void *)request_rpc,
           sizeof *request_rpc);
   request->server = server;
   request->client = client;
   _mongoc_array_init (&request->docs, sizeof (bson_t));

   if (request_rpc->query.opcode != MONGOC_OPCODE_QUERY) {
      /* TODO */
      fprintf (stderr, "TODO: non-query opcodes!\n");
      abort ();
   }

   /* TODO: multiple docs */
   memcpy (&len, request_rpc->query.query, 4);
   len = BSON_UINT32_FROM_LE (len);
   query = bson_new_from_data (request_rpc->query.query, (size_t)len);
   assert (query);
   _mongoc_array_append_val (&request->docs, query);

   if (request_rpc->header.opcode == MONGOC_OPCODE_QUERY &&
       is_command (request->request_rpc.query.collection)) {
      request->is_command = true;

      if (!bson_iter_init (&iter, query) || !bson_iter_next (&iter)) {
         return false;
      }

      request->command_name = bson_strdup (bson_iter_key (&iter));
   }

   return request;
}


/* TODO: take file, line, function params from caller, wrap in macro */
bool
request_matches_query (const request_t     *request,
                       const char          *ns,
                       mongoc_query_flags_t flags,
                       uint32_t             skip,
                       uint32_t             n_return,
                       const char          *query_json,
                       const char          *fields_json)
{
   const mongoc_rpc_t *rpc = &request->request_rpc;
   bson_t *doc;

   assert (request->docs.len <= 2);

   /* TODO: make a good request repr, skip logging and say:
    *   request_t *expected = request_new_from_pattern (...);
    *   if (!request_matches (request, expected)) {
    *       MONGOC_ERROR ("expected %s, got %s",
    *                     request_repr (expected), request_repr (request));
    *       return false;
    *   }
    */
   if (request->is_command) {
      MONGOC_ERROR ("expected query, got command");
      return false;
   }

   if (rpc->header.opcode != MONGOC_OPCODE_QUERY) {
      MONGOC_ERROR ("request's opcode does not match QUERY");
      return false;
   }

   if (strcmp (rpc->query.collection, ns)) {
      MONGOC_ERROR ("request's namespace is '%s', expected '%s'",
                    request->request_rpc.query.collection, ns);
      return false;
   }

   if (rpc->query.flags != flags) {
      MONGOC_ERROR ("request's query flags don't match");
      return false;
   }

   if (rpc->query.skip != skip) {
      MONGOC_ERROR ("requests's skip = %d, expected %d",
                    rpc->query.skip, skip);
      return false;
   }

   if (rpc->query.n_return != n_return) {
      MONGOC_ERROR ("requests's n_return = %d, expected %d",
                    rpc->query.n_return, n_return);
      return false;
   }

   if (request->docs.len) {
      doc = _mongoc_array_index (&request->docs, bson_t *, 0);
   } else {
      doc = NULL;
   }

   if (!match_json (doc, query_json, false, __FILE__, __LINE__, __FUNCTION__)) {
      /* match_json has logged the err */
      return false;
   }

   if (request->docs.len > 1) {
      doc = _mongoc_array_index (&request->docs, bson_t *, 1);
   } else {
      doc = NULL;
   }

   if (!match_json (doc, fields_json, false,
                    __FILE__, __LINE__, __FUNCTION__)) {
      /* match_json has logged the err */
      return false;
   }

   return true;
}


/*--------------------------------------------------------------------------
 *
 * request_destroy --
 *
 *       Free a request_t.
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
request_destroy (request_t *request)
{
   size_t i;
   bson_t *doc;

   for (i = 0; i < request->docs.len; i++) {
      doc = _mongoc_array_index (&request->docs, bson_t *, i);
      bson_destroy (doc);
   }

   _mongoc_array_destroy (&request->docs);
   bson_free ((void *)request->command_name);
   bson_free (request);
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

/* TODO: factor */
static void *
worker_thread (void *data)
{
   worker_closure_t *closure = (worker_closure_t *) data;
   mock_server2_t *server = closure->server;
   mongoc_stream_t *client_stream = closure->client_stream;
   mongoc_buffer_t buffer;
   mongoc_rpc_t *rpc = NULL;
   bool handled;
   bson_error_t error;
   int32_t msg_len;
   bool stopped;
   sync_queue_t *q;
   request_t *request;
   mongoc_array_t autoresponders;
   ssize_t i;
   autoresponder_handle_t handle;

   ENTRY;

   BSON_ASSERT(closure);

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);
   _mongoc_array_init (&autoresponders, sizeof(autoresponder_handle_t));

again:
   bson_free (rpc);
   rpc = NULL;
   handled = false;

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

   /* copies rpc */
   request = request_new (rpc, server, client_stream);

   mongoc_mutex_lock (&server->mutex);
   _mongoc_array_copy (&autoresponders, &server->autoresponders);
   mongoc_mutex_unlock (&server->mutex);

   /* run responders most-recently-added-first */
   for (i = server->autoresponders.len - 1; i >= 0; i--) {
      handle = _mongoc_array_index (&server->autoresponders,
                                    autoresponder_handle_t,
                                    i);
      if (handle.responder(request, handle.data)) {
         handled = true;
         request_destroy (request);
         request = NULL;
         break;
      }
   }

   if (!handled) {
      if (mock_server2_get_verbose (server)) {
         /* TODO: parse and print repr of request */
         /*printf ("%hu\t%s\n", port, "unhandled command");*/
      }

      q = mock_server2_get_queue (server);
      q_put (q, (void *)request);
      request = NULL;
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

   _mongoc_array_destroy (&autoresponders);
   _mongoc_buffer_destroy (&buffer);

   mongoc_stream_close (client_stream);
   mongoc_stream_destroy (client_stream);
   bson_free (rpc);
   bson_free (closure);
   _mongoc_buffer_destroy (&buffer);

   RETURN (NULL);
}

/* TODO: simpler */
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

   n_written = mongoc_stream_writev (client, iov, (size_t)iovcnt, -1);

   assert (n_written == expected);

   _mongoc_array_destroy (&ar);
}


void
autoresponder_handle_destroy (autoresponder_handle_t *handle)
{
   if (handle->destructor) {
      handle->destructor(handle->data);
   }
}
