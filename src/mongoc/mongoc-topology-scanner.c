/*
 * Copyright 2014 MongoDB, Inc.
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

#include "mongoc-error.h"
#include "mongoc-trace.h"
#include "mongoc-topology-scanner-private.h"
#include "mongoc-stream-socket.h"

#ifdef MONGOC_ENABLE_SSL
#include "mongoc-stream-tls.h"
#endif

#include "mongoc-counters-private.h"
#include "mongoc-async-private.h"
#include "mongoc-async-cmd-private.h"
#include "utlist.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology_scanner"

static void
mongoc_topology_scanner_node_destroy (mongoc_topology_scanner_node_t *node);

mongoc_topology_scanner_t *
mongoc_topology_scanner_new (mongoc_topology_scanner_cb_t cb,
                             void                        *cb_data)
{
   mongoc_topology_scanner_t *ts = bson_malloc0 (sizeof (*ts));

   ts->async = mongoc_async_new ();
   bson_init (&ts->ismaster_cmd);
   BSON_APPEND_INT32 (&ts->ismaster_cmd, "isMaster", 1);

   ts->cb = cb;
   ts->cb_data = cb_data;

   return ts;
}

void
mongoc_topology_scanner_destroy (mongoc_topology_scanner_t *ts)
{
   mongoc_topology_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (ts->nodes, ele, tmp) {
      mongoc_topology_scanner_node_destroy (ele);
   }

   mongoc_async_destroy (ts->async);
   bson_destroy (&ts->ismaster_cmd);

   bson_free (ts);
}

void
mongoc_topology_scanner_add (mongoc_topology_scanner_t *ts,
                             const mongoc_host_list_t  *host,
                             uint32_t                   id)
{
   mongoc_topology_scanner_node_t *node = bson_malloc0 (sizeof (*node));

   memcpy (&node->host, host, sizeof (*host));

   node->id = id;
   node->ts = ts;

   DL_APPEND(ts->nodes, node);

   return;
}

static void
mongoc_topology_scanner_node_destroy (mongoc_topology_scanner_node_t *node)
{
   DL_DELETE (node->ts->nodes, node);

   if (node->dns_results) {
      freeaddrinfo (node->dns_results);
      node->dns_results = NULL;
      node->current_dns_result = NULL;
   }

   if (node->cmd) {
      mongoc_async_cmd_destroy (node->cmd);
   }

   if (node->stream) {
      mongoc_stream_destroy (node->stream);
   }

   bson_free (node);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_get_node --
 *
 *      Return the scanner node with the given id.
 *
 *      NOTE: only use this method when single-threaded!
 *
 *--------------------------------------------------------------------------
 */
mongoc_topology_scanner_node_t *
mongoc_topology_scanner_get_node (mongoc_topology_scanner_t *ts,
                                  uint32_t                   id)
{
   mongoc_topology_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (ts->nodes, ele, tmp)
   {
      if (ele->id == id) {
         return ele;
      }

      if (ele->id > id) {
         break;
      }
   }

   return NULL;
}

void
mongoc_topology_scanner_rm (mongoc_topology_scanner_t *ts,
                            uint32_t                   id)
{
   mongoc_topology_scanner_node_t *ele;

   ele = mongoc_topology_scanner_get_node (ts, id);
   if (ele) {
      mongoc_topology_scanner_node_destroy (ele);
   }
}

/*
 *-----------------------------------------------------------------------
 *
 * This is the callback passed to async_cmd when we're running
 * ismasters from within the topology monitor.
 *
 *-----------------------------------------------------------------------
 */

static void
mongoc_topology_scanner_ismaster_handler (mongoc_async_cmd_result_t async_status,
                                          const bson_t             *ismaster_response,
                                          int64_t                   rtt_msec,
                                          void                     *data,
                                          bson_error_t             *error)
{
   mongoc_topology_scanner_node_t *node;

   bson_return_if_fail (data);

   node = (mongoc_topology_scanner_node_t *)data;
   node->cmd = NULL;

   /* if no ismaster response, async cmd had an error or timed out */
   if (!ismaster_response ||
       async_status == MONGOC_ASYNC_CMD_ERROR ||
       async_status == MONGOC_ASYNC_CMD_TIMEOUT) {
      mongoc_stream_destroy (node->stream);
      node->stream = NULL;
   }

   if (!node->ts->cb (node->id, ismaster_response, rtt_msec,
                      node->ts->cb_data, error)) {
      mongoc_topology_scanner_node_destroy (node);
      return;
   }
}

static mongoc_stream_t *
mongoc_topology_scanner_node_connect_tcp (mongoc_topology_scanner_node_t *node,
                                          bson_error_t                   *error)
{
   mongoc_socket_t *sock = NULL;
   struct addrinfo hints;
   struct addrinfo *rp;
   char portstr [8];
   mongoc_host_list_t *host;
   int s;

   ENTRY;

   host = &node->host;

   if (!node->dns_results) {
      bson_snprintf (portstr, sizeof portstr, "%hu", host->port);

      memset (&hints, 0, sizeof hints);
      hints.ai_family = host->family;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = 0;
      hints.ai_protocol = 0;

      s = getaddrinfo (host->host, portstr, &hints, &node->dns_results);

      if (s != 0) {
         mongoc_counter_dns_failure_inc ();
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                         "Failed to resolve %s",
                         host->host);
         RETURN (NULL);
      }

      node->current_dns_result = node->dns_results;

      mongoc_counter_dns_success_inc ();
   }

   for (; node->current_dns_result;
        node->current_dns_result = node->current_dns_result->ai_next) {
      rp = node->current_dns_result;

      /*
       * Create a new non-blocking socket.
       */
      if (!(sock = mongoc_socket_new (rp->ai_family,
                                      rp->ai_socktype,
                                      rp->ai_protocol))) {
         continue;
      }

      mongoc_socket_connect (sock, rp->ai_addr, (socklen_t)rp->ai_addrlen, 0);

      break;
   }

   if (!sock) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      "Failed to connect to target host: %s",
                      host->host_and_port);
      freeaddrinfo (node->dns_results);
      node->dns_results = NULL;
      node->current_dns_result = NULL;
      RETURN (NULL);
   }

   return mongoc_stream_socket_new (sock);
}

static mongoc_stream_t *
mongoc_topology_scanner_node_connect_unix (mongoc_topology_scanner_node_t *node,
                                           bson_error_t                   *error)
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
   mongoc_host_list_t *host;

   ENTRY;

   host = &node->host;

   memset (&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   bson_snprintf (saddr.sun_path, sizeof saddr.sun_path - 1,
                  "%s", host->host_and_port);

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

static bool
mongoc_topology_scanner_node_setup (mongoc_topology_scanner_node_t *node)
{
   mongoc_stream_t *sock_stream;
   bson_error_t error;

   if (node->stream) { return true; }

   /* TODO hook up a callback for this so you can use your own stream initiator */

   if (node->host.family == AF_UNIX) {
      sock_stream = mongoc_topology_scanner_node_connect_unix (node, &error);
   } else {
      sock_stream = mongoc_topology_scanner_node_connect_tcp (node, &error);
   }

   if (!sock_stream) {
      if (!node->ts->cb (node->id, NULL, 0, node->ts->cb_data, &error)) {
         mongoc_topology_scanner_node_destroy (node);
      }

      return false;
   }

#ifdef MONGOC_ENABLE_SSL

   if (node->ts->ssl_opts) {
      sock_stream = mongoc_stream_tls_new (sock_stream, node->ts->ssl_opts, 1);
   }

#endif

   node->stream = sock_stream;
   node->has_auth = false;
   node->timestamp = bson_get_monotonic_time ();

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_start --
 *
 *      Initializes the scanner and begins a full topology check. This
 *      should be called once before calling mongoc_topology_scanner_work()
 *      repeatedly to complete the scan.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_scanner_start (mongoc_topology_scanner_t *ts,
                               int32_t                timeout_msec)
{
   mongoc_topology_scanner_node_t *node, *tmp;

   bson_return_if_fail (ts);

   if (ts->in_progress) {
      return;
   }

   DL_FOREACH_SAFE (ts->nodes, node, tmp)
   {
      if (mongoc_topology_scanner_node_setup (node)) {
         node->cmd = mongoc_async_cmd (ts->async, node->stream, "admin",
                                       &ts->ismaster_cmd,
                                       &mongoc_topology_scanner_ismaster_handler,
                                       node,
                                       timeout_msec);
      }
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_work --
 *
 *      Crank the knob on the topology scanner state machine. This should
 *      be called only after mongoc_topology_scanner_start() has been used
 *      to begin the scan.
 *
 * Returns:
 *      true if there is more work to do, false if scan is done.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_topology_scanner_work (mongoc_topology_scanner_t *ts,
                              int32_t                    timeout_msec)
{
   bool r;

   r = mongoc_async_run (ts->async, timeout_msec);

   if (! r) {
      ts->in_progress = false;
   }

   return r;
}
