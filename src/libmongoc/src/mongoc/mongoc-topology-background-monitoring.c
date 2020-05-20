/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "mongoc-topology-background-monitoring-private.h"

#include "mongoc-client-private.h"
#include "mongoc-log-private.h"
#ifdef MONGOC_ENABLE_SSL
#include "mongoc-ssl-private.h"
#endif
#include "mongoc-stream-private.h"
#include "mongoc-topology-description-apm-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-util-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "bg_monitor"

static BSON_THREAD_FUN (srv_polling_run, topology_void)
{
   mongoc_topology_t *topology;

   topology = topology_void;
   bson_mutex_lock (&topology->mutex);
   while (true) {
      int64_t now_ms;
      int64_t scan_due_ms;
      int64_t sleep_duration_ms;

      if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
         bson_mutex_unlock (&topology->mutex);
         break;
      }

      /* This will check if a scan is due. */
      mongoc_topology_rescan_srv (topology);

      /* Unlock and sleep until next scan is due, or until shutdown signalled.
       */
      now_ms = bson_get_monotonic_time () / 1000;
      scan_due_ms = topology->srv_polling_last_scan_ms +
                    topology->srv_polling_rescan_interval_ms;
      sleep_duration_ms = scan_due_ms - now_ms;

      if (sleep_duration_ms > 0) {
         TRACE ("srv polling thread sleeping for %" PRId64 "ms",
                sleep_duration_ms);
      }

      /* If shutting down, stop. */
      mongoc_cond_timedwait (
         &topology->srv_polling_cond, &topology->mutex, sleep_duration_ms);
   }
   BSON_THREAD_RETURN;
}

typedef struct {
   mongoc_topology_t *topology;
   bson_thread_t thread;

   /* State accessed from multiple threads. */
   struct {
      bson_mutex_t mutex;
      mongoc_cond_t cond;
      bool shutting_down;
      bool is_shutdown;
      bool scan_requested;
   } shared;

   /* Time of last scan in milliseconds. */
   uint64_t last_scan_ms;
   /* The time of the next scheduled scan. */
   uint64_t scan_due_ms;
   /* The server id matching the server description. */
   uint32_t server_id;
   /* Default time to sleep between ismaster checks (reduced when a scan is
    * requested) */
   uint64_t heartbeat_frequency_ms;
   /* The minimum time to sleep between ismaster checks. */
   uint64_t min_heartbeat_frequency_ms;
   int64_t connect_timeout_ms;
   bool use_tls;
#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t *ssl_opts;
#endif
   mongoc_uri_t *uri;
   mongoc_host_list_t host;
   /* A custom initiator may be set if a user provides overrides to create a
    * stream. */
   mongoc_stream_initiator_t initiator;
   void *initiator_context;
   mongoc_stream_t *stream;
   int64_t request_id;
   mongoc_apm_callbacks_t apm_callbacks;
   void *apm_context;
} mongoc_server_monitor_t;

/* Called only from server monitor thread.
 * Caller must hold no locks (user's callback may lock topology mutex).
 * Locks APM mutex.
 */
static void
_server_monitor_heartbeat_started (mongoc_server_monitor_t *server_monitor)
{
   mongoc_apm_server_heartbeat_started_t event;
   if (!server_monitor->apm_callbacks.server_heartbeat_started) {
      return;
   }

   event.host = &server_monitor->host;
   event.context = server_monitor->apm_context;
   bson_mutex_lock (&server_monitor->topology->apm_mutex);
   server_monitor->apm_callbacks.server_heartbeat_started (&event);
   bson_mutex_unlock (&server_monitor->topology->apm_mutex);
}

/* Called only from server monitor thread.
 * Caller must hold no locks (user's callback may lock topology mutex).
 * Locks APM mutex.
 */
static void
_server_monitor_heartbeat_succeeded (mongoc_server_monitor_t *server_monitor,
                                     const bson_t *reply,
                                     int64_t duration_usec)
{
   mongoc_apm_server_heartbeat_succeeded_t event;

   if (!server_monitor->apm_callbacks.server_heartbeat_succeeded) {
      return;
   }
   event.host = &server_monitor->host;
   event.context = server_monitor->apm_context;
   event.reply = reply;
   event.duration_usec = duration_usec;
   bson_mutex_lock (&server_monitor->topology->apm_mutex);
   server_monitor->apm_callbacks.server_heartbeat_succeeded (&event);
   bson_mutex_unlock (&server_monitor->topology->apm_mutex);
}

/* Called only from server monitor thread.
 * Caller must hold no locks (user's callback may lock topology mutex).
 * Locks APM mutex.
 */
static void
_server_monitor_heartbeat_failed (mongoc_server_monitor_t *server_monitor,
                                  const bson_error_t *error,
                                  int64_t duration_usec)
{
   mongoc_apm_server_heartbeat_failed_t event;

   if (!server_monitor->apm_callbacks.server_heartbeat_failed) {
      return;
   }

   event.host = &server_monitor->host;
   event.context = server_monitor->apm_context;
   event.error = error;
   event.duration_usec = duration_usec;
   bson_mutex_lock (&server_monitor->topology->apm_mutex);
   server_monitor->apm_callbacks.server_heartbeat_failed (&event);
   bson_mutex_unlock (&server_monitor->topology->apm_mutex);
}

static bool
_server_monitor_cmd_send (mongoc_server_monitor_t *server_monitor,
                          bson_t *cmd,
                          bson_t *reply,
                          bson_error_t *error)
{
   mongoc_rpc_t rpc;
   mongoc_array_t array_to_write;
   mongoc_iovec_t *iovec;
   int niovec;
   mongoc_buffer_t buffer;
   uint32_t reply_len;
   bson_t temp_reply;

   rpc.header.msg_len = 0;
   rpc.header.request_id = server_monitor->request_id++;
   rpc.header.response_to = 0;
   rpc.header.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   rpc.query.collection = "admin.$cmd";
   rpc.query.skip = 0;
   rpc.query.n_return = -1;
   rpc.query.query = bson_get_data (cmd);
   rpc.query.fields = NULL;

   _mongoc_array_init (&array_to_write, sizeof (mongoc_iovec_t));
   _mongoc_rpc_gather (&rpc, &array_to_write);
   iovec = (mongoc_iovec_t *) array_to_write.data;
   niovec = array_to_write.len;
   _mongoc_rpc_swab_to_le (&rpc);

   if (!_mongoc_stream_writev_full (server_monitor->stream,
                                    iovec,
                                    niovec,
                                    server_monitor->connect_timeout_ms,
                                    error)) {
      _mongoc_array_destroy (&array_to_write);
      bson_init (reply);
      return false;
   }
   _mongoc_array_destroy (&array_to_write);

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);
   if (!_mongoc_buffer_append_from_stream (&buffer,
                                           server_monitor->stream,
                                           4,
                                           server_monitor->connect_timeout_ms,
                                           error)) {
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   memcpy (&reply_len, buffer.data, 4);
   reply_len = BSON_UINT32_FROM_LE (reply_len);

   if (!_mongoc_buffer_append_from_stream (&buffer,
                                           server_monitor->stream,
                                           reply_len - buffer.len,
                                           server_monitor->connect_timeout_ms,
                                           error)) {
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   if (!_mongoc_rpc_scatter (&rpc, buffer.data, buffer.len)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid reply from server.");

      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }

   if (BSON_UINT32_FROM_LE (rpc.header.opcode) == MONGOC_OPCODE_COMPRESSED) {
      uint8_t *buf = NULL;
      size_t len = BSON_UINT32_FROM_LE (rpc.compressed.uncompressed_size) +
                   sizeof (mongoc_rpc_header_t);

      buf = bson_malloc0 (len);
      if (!_mongoc_rpc_decompress (&rpc, buf, len)) {
         bson_free (buf);
         _mongoc_buffer_destroy (&buffer);
         bson_init (reply);
         bson_set_error (error,
                         MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         "Could not decompress server reply");
         return MONGOC_ASYNC_CMD_ERROR;
      }

      _mongoc_buffer_destroy (&buffer);
      _mongoc_buffer_init (&buffer, buf, len, NULL, NULL);
   }

   _mongoc_rpc_swab_from_le (&rpc);

   if (!_mongoc_rpc_get_first_document (&rpc, &temp_reply)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid reply from server");
      _mongoc_buffer_destroy (&buffer);
      bson_init (reply);
      return false;
   }
   bson_copy_to (&temp_reply, reply);
   _mongoc_buffer_destroy (&buffer);
   return true;
}

/* Update the topology description with a reply or an error.
 *
 * Called only from server monitor thread.
 * Caller must hold no locks.
 * Locks topology mutex.
 */
static void
_server_monitor_update_topology_description (
   mongoc_server_monitor_t *server_monitor,
   bson_t *reply,
   uint64_t rtt_us,
   bson_error_t *error)
{
   mongoc_topology_t *topology;

   topology = server_monitor->topology;
   bson_mutex_lock (&topology->mutex);
   if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN) {
      mongoc_topology_description_handle_ismaster (
         &server_monitor->topology->description,
         server_monitor->server_id,
         reply,
         rtt_us / 1000,
         error);
      /* reconcile server monitors. */
      _mongoc_topology_background_monitoring_reconcile (topology);
   }
   /* Wake threads performing server selection. */
   mongoc_cond_broadcast (&server_monitor->topology->cond_client);
   bson_mutex_unlock (&server_monitor->topology->mutex);
}

/* Send an ismaster command to a server.
 *
 * Called only from server monitor thread.
 * Caller must hold no locks.
 * Locks server_monitor->mutex to reset scan_requested.
 * Locks topology mutex when updating topology description with new ismaster
 * reply (or error).
 */
static void
_server_monitor_regular_ismaster (mongoc_server_monitor_t *server_monitor)
{
   bson_t cmd;
   bson_t reply;
   bool ret;
   bson_error_t error = {0};
   int64_t rtt_us;
   int64_t start_us;
   int attempt;
   bool stream_established = false;
   mongoc_topology_t *topology;

   topology = server_monitor->topology;
   bson_init (&cmd);
   bson_init (&reply);
   rtt_us = 0;
   attempt = 0;
   while (true) {
      /* If the first attempt to send an ismaster failed, see if we can attempt
       * again.
       * Server monitoring spec: Once the server is connected, the client MUST
       * change its type to Unknown only after it has retried the server once.
      */
      if (attempt > 0) {
         mongoc_server_description_t *existing_sd;
         bool should_retry = false;

         if (attempt > 1 || !stream_established) {
            /* We've already retried, or we were not able to establish a
             * connection at all. */
            should_retry = false;
         } else {
            bson_mutex_lock (&topology->mutex);
            /* If the server description is already Unknown, don't retry. */
            if (topology->scanner_state !=
                MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN) {
               existing_sd = mongoc_topology_description_server_by_id (
                  &topology->description, server_monitor->server_id, NULL);
               if (existing_sd != NULL &&
                   existing_sd->type != MONGOC_SERVER_UNKNOWN) {
                  should_retry = true;
               }
            }

            bson_mutex_unlock (&server_monitor->topology->mutex);
         }

         if (!should_retry) {
            TRACE ("sm (%d): cannot retry", server_monitor->server_id);
            /* error was previously set. */
            _server_monitor_update_topology_description (
               server_monitor, NULL, -1, &error);
            break;
         } else {
            TRACE ("sm (%d): going to retry", server_monitor->server_id);
         }
      }
      attempt++;

      bson_reinit (&cmd);
      BCON_APPEND (&cmd, "isMaster", BCON_INT32 (1));

      if (!server_monitor->stream) {
         /* Using an initiator isn't really necessary. Users can't set them on
          * pools. But it is used for tests. */
         if (server_monitor->initiator) {
            server_monitor->stream =
               server_monitor->initiator (server_monitor->uri,
                                          &server_monitor->host,
                                          server_monitor->initiator_context,
                                          &error);
         } else {
            void *ssl_opts_void = NULL;

#ifdef MONGOC_ENABLE_SSL
            ssl_opts_void = server_monitor->ssl_opts;
#endif
            server_monitor->stream =
               mongoc_client_connect (false,
                                      ssl_opts_void != NULL,
                                      ssl_opts_void,
                                      server_monitor->uri,
                                      &server_monitor->host,
                                      &error);
         }
         if (!server_monitor->stream) {
            TRACE ("sm (%d) failed to connect", server_monitor->server_id);
            _server_monitor_heartbeat_failed (server_monitor, &error, rtt_us);
            continue;
         }

         bson_destroy (&cmd);
         bson_copy_to (_mongoc_topology_get_ismaster (server_monitor->topology),
                       &cmd);
      }

      /* A stream was already established, or we created one successfully. (This
       * permits a retry). */
      stream_established = true;

      /* Cluster time is updated on every reply. Don't wait for notifications,
       * just poll it. */
      bson_mutex_lock (&server_monitor->topology->mutex);
      if (!bson_empty (&server_monitor->topology->description.cluster_time)) {
         bson_append_document (
            &cmd,
            "$clusterTime",
            12,
            &server_monitor->topology->description.cluster_time);
      }
      bson_mutex_unlock (&server_monitor->topology->mutex);

      start_us = bson_get_monotonic_time ();
      bson_destroy (&reply);
      _server_monitor_heartbeat_started (server_monitor);
      ret = _server_monitor_cmd_send (server_monitor, &cmd, &reply, &error);
      if (!ret) {
         TRACE ("sm (%d) error = %s", server_monitor->server_id, error.message);
      }
      /* Must mark scan_requested as "delivered" before updating the topology
       * description, not after. Otherwise, we could miss a scan request in
       * server selection. We need to uphold the invariant: if a scan is
       * requested, cond_client will be signalled.
       */
      bson_mutex_lock (&server_monitor->shared.mutex);
      server_monitor->shared.scan_requested = false;
      bson_mutex_unlock (&server_monitor->shared.mutex);
      rtt_us = (bson_get_monotonic_time () - start_us);

      if (ret) {
         _server_monitor_update_topology_description (
            server_monitor, &reply, rtt_us, &error);
         _server_monitor_heartbeat_succeeded (server_monitor, &reply, rtt_us);
         break;
      } else {
         mongoc_stream_destroy (server_monitor->stream);
         server_monitor->stream = NULL;
         _server_monitor_heartbeat_failed (server_monitor, &error, rtt_us);
         continue;
      }
   }

   bson_destroy (&cmd);
   bson_destroy (&reply);
}

/* The server monitor thread.
 *
 * Runs continuously and sends ismaster commands. Sleeps until it is time to
 * scan or woken by a change in shared state:
 * - a request for immediate scan
 * - a request for shutdown
 * Locks the server mutex to check shared state.
 * Locks topology mutex to update description.
 */
static BSON_THREAD_FUN (_server_monitor_run, server_monitor_void)
{
   mongoc_server_monitor_t *server_monitor;

   server_monitor = (mongoc_server_monitor_t *) server_monitor_void;

   while (true) {
      int64_t now_ms;
      int64_t sleep_duration_ms;

      now_ms = bson_get_monotonic_time () / 1000;
      if (now_ms >= server_monitor->scan_due_ms) {
         TRACE ("sm (%d) sending ismaster", server_monitor->server_id);
         _server_monitor_regular_ismaster (server_monitor);
         server_monitor->last_scan_ms = bson_get_monotonic_time () / 1000;
         server_monitor->scan_due_ms = server_monitor->last_scan_ms +
                                       server_monitor->heartbeat_frequency_ms;
      }

      bson_mutex_lock (&server_monitor->shared.mutex);
      if (server_monitor->shared.shutting_down) {
         server_monitor->shared.is_shutdown = true;
         bson_mutex_unlock (&server_monitor->shared.mutex);
         break;
      }

      if (server_monitor->shared.scan_requested) {
         server_monitor->scan_due_ms =
            server_monitor->last_scan_ms +
            server_monitor->min_heartbeat_frequency_ms;
      }

      sleep_duration_ms = server_monitor->scan_due_ms - now_ms;

      if (sleep_duration_ms > 0) {
         TRACE ("sm (%d) sleeping for %" PRId64,
                server_monitor->server_id,
                sleep_duration_ms);
         mongoc_cond_timedwait (&server_monitor->shared.cond,
                                &server_monitor->shared.mutex,
                                sleep_duration_ms);
      }
      bson_mutex_unlock (&server_monitor->shared.mutex);
   }
   BSON_THREAD_RETURN;
}

/* Free data for a server monitor.
 *
 * Called from any thread during reconcile and during the shutdown procedure.
 * Caller must have topology mutex locked, but not the server monitor mutex.
 */
static void
_server_monitor_destroy (mongoc_server_monitor_t *server_monitor)
{
   mongoc_stream_destroy (server_monitor->stream);
   mongoc_uri_destroy (server_monitor->uri);
   mongoc_cond_destroy (&server_monitor->shared.cond);
   bson_mutex_destroy (&server_monitor->shared.mutex);
#ifdef MONGOC_ENABLE_SSL
   if (server_monitor->ssl_opts) {
      _mongoc_ssl_opts_cleanup (server_monitor->ssl_opts, true);
      bson_free (server_monitor->ssl_opts);
   }
#endif
   bson_free (server_monitor);
}

/* Signal a server monitor to shutdown. If shutdown has already completed, frees
 * server monitor and returns true.
 *
 * Called only during topology description reconcile (from any thread).
 * Caller must hold topology lock.
 */
static bool
_server_monitor_try_shutdown_and_destroy (
   mongoc_server_monitor_t *server_monitor)
{
   bool is_shutdown;

   bson_mutex_lock (&server_monitor->shared.mutex);
   is_shutdown = server_monitor->shared.is_shutdown;
   server_monitor->shared.shutting_down = true;
   mongoc_cond_signal (&server_monitor->shared.cond);
   bson_mutex_unlock (&server_monitor->shared.mutex);

   /* If the server monitor thread has exited, join so the internal thread state
    * can be freed. Joining can only be done once the server monitor has
    * completed shutdown. Otherwise, the server monitor may be in the middle of
    * scanning (and therefore may need to take the topology mutex again).
    *
    * Since the topology mutex is locked, we're guaranteed that only one thread
    * will join.
    */
   if (is_shutdown) {
      TRACE ("sm (%d) try join start", server_monitor->server_id);
      COMMON_PREFIX (thread_join) (server_monitor->thread);
      TRACE ("sm (%d) try join end", server_monitor->server_id);
      _server_monitor_destroy (server_monitor);
      return true;
   }
   return false; /* Still waiting for shutdown. */
}

/* Request scan of a single server.
 *
 * Caller does not need to have topology mutex locked.
 * Locks server_monitor mutex to deliver scan_requested.
 */
static void
_server_monitor_request_scan (mongoc_server_monitor_t *server_monitor)
{
   bson_mutex_lock (&server_monitor->shared.mutex);
   server_monitor->shared.scan_requested = true;
   mongoc_cond_signal (&server_monitor->shared.cond);
   bson_mutex_unlock (&server_monitor->shared.mutex);
}

/* Create a server monitor if necessary.
 *
 * Called by monitor threads and application threads when reconciling the
 * topology description. Caller must have topology mutex locked.
 */
static void
_background_monitor_reconcile_server_monitor (mongoc_topology_t *topology,
                                              mongoc_server_description_t *sd)
{
   mongoc_set_t *server_monitors;
   mongoc_server_monitor_t *server_monitor;

   server_monitors = topology->server_monitors;
   server_monitor = mongoc_set_get (server_monitors, sd->id);

   if (server_monitor) {
      return;
   }

   /* Add a new server monitor. */
   server_monitor = bson_malloc0 (sizeof (*server_monitor));
   server_monitor->server_id = sd->id;
   memcpy (&server_monitor->host, &sd->host, sizeof (mongoc_host_list_t));
   server_monitor->topology = topology;
   server_monitor->heartbeat_frequency_ms =
      topology->description.heartbeat_msec;
   server_monitor->min_heartbeat_frequency_ms =
      topology->min_heartbeat_frequency_msec;
   server_monitor->connect_timeout_ms = topology->connect_timeout_msec;
   server_monitor->uri = mongoc_uri_copy (topology->uri);
/* TODO: CDRIVER-3682 Do not retrieve ssl opts from topology scanner. They
 * should be stored somewhere else. */
#ifdef MONGOC_ENABLE_SSL
   if (topology->scanner->ssl_opts) {
      server_monitor->ssl_opts = bson_malloc0 (sizeof (mongoc_ssl_opt_t));

      _mongoc_ssl_opts_copy_to (
         topology->scanner->ssl_opts, server_monitor->ssl_opts, true);
   }
#endif
   memcpy (&server_monitor->apm_callbacks,
           &topology->description.apm_callbacks,
           sizeof (mongoc_apm_callbacks_t));
   server_monitor->apm_context = topology->description.apm_context;
   server_monitor->initiator = topology->scanner->initiator;
   server_monitor->initiator_context = topology->scanner->initiator_context;
   mongoc_cond_init (&server_monitor->shared.cond);
   bson_mutex_init (&server_monitor->shared.mutex);
   COMMON_PREFIX (thread_create)
   (&server_monitor->thread, _server_monitor_run, server_monitor);
   mongoc_set_add (server_monitors, server_monitor->server_id, server_monitor);
}

/* Start background monitoring.
 *
 * Called by an application thread popping a client from a pool. Safe to
 * call repeatedly.
 * Caller must have topology mutex locked.
 */
void
_mongoc_topology_background_monitoring_start (mongoc_topology_t *topology)
{
   BSON_ASSERT (!topology->single_threaded);

   if (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      return;
   }

   BSON_ASSERT (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_OFF);

   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_BG_RUNNING;

   _mongoc_handshake_freeze ();
   _mongoc_topology_description_monitor_opening (&topology->description);

   /* Reconcile to create the first server monitors. */
   _mongoc_topology_background_monitoring_reconcile (topology);
   /* Start SRV polling thread. */
   if (mongoc_uri_get_service (topology->uri)) {
      COMMON_PREFIX (thread_create)
      (&topology->srv_polling_thread, srv_polling_run, topology);
   }
}

/* Reconcile the topology description with the set of server monitors.
 *
 * Called when the topology description is updated (via handshake, monitoring,
 * or invalidation). May be called by server monitor thread or an application
 * thread.
 * Caller must have topology mutex locked.
 * Locks server monitor mutexes. May join / remove server monitors that have
 * completed shutdown.
 */
void
_mongoc_topology_background_monitoring_reconcile (mongoc_topology_t *topology)
{
   mongoc_topology_description_t *td;
   mongoc_set_t *server_descriptions;
   mongoc_set_t *server_monitors;
   uint32_t *server_monitor_ids_to_remove;
   uint32_t n_server_monitor_ids_to_remove = 0;
   int i;

   td = &topology->description;
   server_descriptions = td->servers;
   server_monitors = topology->server_monitors;

   BSON_ASSERT (!topology->single_threaded);

   if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      return;
   }

   /* Add newly discovered server monitors, and update existing ones. */
   for (i = 0; i < server_descriptions->items_len; i++) {
      mongoc_server_description_t *sd;

      sd = mongoc_set_get_item (server_descriptions, i);
      _background_monitor_reconcile_server_monitor (topology, sd);
   }

   /* Signal shutdown to server monitors no longer in the topology description.
    */
   server_monitor_ids_to_remove =
      bson_malloc0 (sizeof (uint32_t) * server_monitors->items_len);
   for (i = 0; i < server_monitors->items_len; i++) {
      mongoc_server_monitor_t *server_monitor;
      uint32_t id;

      server_monitor = mongoc_set_get_item_and_id (server_monitors, i, &id);
      if (!mongoc_set_get (server_descriptions, id)) {
         if (_server_monitor_try_shutdown_and_destroy (server_monitor)) {
            server_monitor_ids_to_remove[n_server_monitor_ids_to_remove] = id;
            n_server_monitor_ids_to_remove++;
         }
      }
   }

   /* Remove freed server monitors that have completed shutdown. */
   for (i = 0; i < n_server_monitor_ids_to_remove; i++) {
      mongoc_set_rm (server_monitors, server_monitor_ids_to_remove[i]);
   }
   bson_free (server_monitor_ids_to_remove);
}

/* Request all server monitors to scan.
 *
 * Called from application threads (during server selection or "not master"
 * errors). Caller must have topology mutex locked. Locks server monitor mutexes
 * to deliver scan_requested.
 */
void
_mongoc_topology_background_monitoring_request_scan (
   mongoc_topology_t *topology)
{
   mongoc_set_t *server_monitors;
   int i;

   BSON_ASSERT (!topology->single_threaded);

   if (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN) {
      return;
   }

   server_monitors = topology->server_monitors;

   for (i = 0; i < server_monitors->items_len; i++) {
      mongoc_server_monitor_t *server_monitor;
      uint32_t id;

      server_monitor = mongoc_set_get_item_and_id (server_monitors, i, &id);
      _server_monitor_request_scan (server_monitor);
   }
}

/* Stop, join, and destroy all server monitors.
 *
 * Called by application threads when destroying a client pool.
 * Caller must have topology mutex locked.
 * Locks server monitor mutexes to deliver shutdown. Releases topology mutex to
 * join server monitor threads. Leaves topology mutex locked on exit. This
 * function is thread-safe. But in practice, it is only ever called by one
 * application thread (because mongoc_client_pool_destroy is not thread-safe).
 */
void
_mongoc_topology_background_monitoring_stop (mongoc_topology_t *topology)
{
   mongoc_server_monitor_t *server_monitor;
   int i;
   bool is_srv_polling;

   BSON_ASSERT (!topology->single_threaded);

   if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      return;
   }

   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN;

   is_srv_polling = NULL != mongoc_uri_get_service (topology->uri);
   /* Signal SRV polling to shut down (if it is started). */
   if (is_srv_polling) {
      mongoc_cond_signal (&topology->srv_polling_cond);
   }

   /* Signal all server monitors to shut down. */
   for (i = 0; i < topology->server_monitors->items_len; i++) {
      server_monitor = mongoc_set_get_item (topology->server_monitors, i);
      bson_mutex_lock (&server_monitor->shared.mutex);
      server_monitor->shared.shutting_down = true;
      mongoc_cond_signal (&server_monitor->shared.cond);
      bson_mutex_unlock (&server_monitor->shared.mutex);
   }

   /* Some mongoc_server_monitor_t may be waiting for topology mutex. Unlock so
    * they can proceed to terminate. It is safe to unlock topology mutex. Since
    * scanner_state has transitioned to shutting down, no thread can modify
    * server_monitors. */
   bson_mutex_unlock (&topology->mutex);

   for (i = 0; i < topology->server_monitors->items_len; i++) {
      /* Wait for the thread to shutdown. */
      server_monitor = mongoc_set_get_item (topology->server_monitors, i);
      TRACE ("sm (%d) joining thread", server_monitor->server_id);
      COMMON_PREFIX (thread_join) (server_monitor->thread);
      TRACE ("sm (%d) thread joined", server_monitor->server_id);
      _server_monitor_destroy (server_monitor);
   }

   /* Wait for SRV polling thread. */
   if (is_srv_polling) {
      COMMON_PREFIX (thread_join) (topology->srv_polling_thread);
   }

   bson_mutex_lock (&topology->mutex);
   mongoc_set_destroy (topology->server_monitors);
   topology->server_monitors = mongoc_set_new (1, NULL, NULL);
   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_OFF;
   mongoc_cond_broadcast (&topology->cond_client);
}
