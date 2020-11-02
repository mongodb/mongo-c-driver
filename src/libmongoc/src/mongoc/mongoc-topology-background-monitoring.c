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
#include "mongoc-server-monitor-private.h"
#ifdef MONGOC_ENABLE_SSL
#include "mongoc-ssl-private.h"
#endif
#include "mongoc-stream-private.h"
#include "mongoc-topology-description-apm-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-util-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "monitor"

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
      if (!mongoc_topology_should_rescan_srv (topology)) {
         TRACE ("%s\n", "topology ineligible for SRV polling, stopping");
         bson_mutex_unlock (&topology->mutex);
         break;
      }

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

      /* Check for shutdown again here. mongoc_topology_rescan_srv unlocks the
       * topology mutex for the scan. The topology may have shut down in that
       * time. */
      if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
         bson_mutex_unlock (&topology->mutex);
         break;
      }

      /* If shutting down, stop. */
      mongoc_cond_timedwait (
         &topology->srv_polling_cond, &topology->mutex, sleep_duration_ms);
   }
   BSON_THREAD_RETURN;
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

   if (!server_monitor) {
      /* Add a new server monitor. */
      server_monitor = mongoc_server_monitor_new (topology, sd);
      mongoc_server_monitor_run (server_monitor);
      mongoc_set_add (server_monitors, sd->id, server_monitor);
   }

   /* Check if an RTT monitor is needed. */
   if (!bson_empty (&sd->topology_version)) {
      mongoc_set_t *rtt_monitors;
      mongoc_server_monitor_t *rtt_monitor;

      rtt_monitors = topology->rtt_monitors;
      rtt_monitor = mongoc_set_get (rtt_monitors, sd->id);
      if (!rtt_monitor) {
         rtt_monitor = mongoc_server_monitor_new (topology, sd);
         mongoc_server_monitor_run_as_rtt (rtt_monitor);
         mongoc_set_add (rtt_monitors, sd->id, rtt_monitor);
      }
   }
   return;
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

   TRACE ("%s", "background monitoring starting");

   BSON_ASSERT (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_OFF);

   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_BG_RUNNING;

   _mongoc_handshake_freeze ();
   _mongoc_topology_description_monitor_opening (&topology->description);

   /* Reconcile to create the first server monitors. */
   _mongoc_topology_background_monitoring_reconcile (topology);
   /* Start SRV polling thread. */
   if (mongoc_topology_should_rescan_srv (topology)) {
      topology->is_srv_polling = true;
      COMMON_PREFIX (thread_create)
      (&topology->srv_polling_thread, srv_polling_run, topology);
   }
}

/* Remove server monitors that are no longer in the set of server descriptions.
 *
 * Called by monitor threads and application threads when reconciling the
 * topology description. Caller must have topology mutex locked.
 */
static void
_remove_orphaned_server_monitors (mongoc_set_t *server_monitors,
                                  mongoc_set_t *server_descriptions)
{
   uint32_t *server_monitor_ids_to_remove;
   uint32_t n_server_monitor_ids_to_remove = 0;
   int i;

   /* Signal shutdown to server monitors no longer in the topology description.
    */
   server_monitor_ids_to_remove =
      bson_malloc0 (sizeof (uint32_t) * server_monitors->items_len);
   for (i = 0; i < server_monitors->items_len; i++) {
      mongoc_server_monitor_t *server_monitor;
      uint32_t id;

      server_monitor = mongoc_set_get_item_and_id (server_monitors, i, &id);
      if (!mongoc_set_get (server_descriptions, id)) {
         if (mongoc_server_monitor_request_shutdown (server_monitor)) {
            mongoc_server_monitor_wait_for_shutdown (server_monitor);
            mongoc_server_monitor_destroy (server_monitor);
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
   int i;

   td = &topology->description;
   server_descriptions = td->servers;

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

   _remove_orphaned_server_monitors (topology->server_monitors,
                                     server_descriptions);
   _remove_orphaned_server_monitors (topology->rtt_monitors,
                                     server_descriptions);
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
      mongoc_server_monitor_request_scan (server_monitor);
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

   BSON_ASSERT (!topology->single_threaded);

   if (topology->scanner_state != MONGOC_TOPOLOGY_SCANNER_BG_RUNNING) {
      return;
   }

   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN;
   TRACE ("%s", "background monitoring stopping");

   /* Signal SRV polling to shut down (if it is started). */
   if (topology->is_srv_polling) {
      mongoc_cond_signal (&topology->srv_polling_cond);
   }

   /* Signal all server monitors to shut down. */
   for (i = 0; i < topology->server_monitors->items_len; i++) {
      server_monitor = mongoc_set_get_item (topology->server_monitors, i);
      mongoc_server_monitor_request_shutdown (server_monitor);
   }

   /* Signal all RTT monitors to shut down. */
   for (i = 0; i < topology->rtt_monitors->items_len; i++) {
      server_monitor = mongoc_set_get_item (topology->rtt_monitors, i);
      mongoc_server_monitor_request_shutdown (server_monitor);
   }

   /* Some mongoc_server_monitor_t may be waiting for topology mutex. Unlock so
    * they can proceed to terminate. It is safe to unlock topology mutex. Since
    * scanner_state has transitioned to shutting down, no thread can modify
    * server_monitors. */
   bson_mutex_unlock (&topology->mutex);

   for (i = 0; i < topology->server_monitors->items_len; i++) {
      /* Wait for the thread to shutdown. */
      server_monitor = mongoc_set_get_item (topology->server_monitors, i);
      mongoc_server_monitor_wait_for_shutdown (server_monitor);
      mongoc_server_monitor_destroy (server_monitor);
   }

   for (i = 0; i < topology->rtt_monitors->items_len; i++) {
      /* Wait for the thread to shutdown. */
      server_monitor = mongoc_set_get_item (topology->rtt_monitors, i);
      mongoc_server_monitor_wait_for_shutdown (server_monitor);
      mongoc_server_monitor_destroy (server_monitor);
   }

   /* Wait for SRV polling thread. */
   if (topology->is_srv_polling) {
      COMMON_PREFIX (thread_join) (topology->srv_polling_thread);
   }

   bson_mutex_lock (&topology->mutex);
   mongoc_set_destroy (topology->server_monitors);
   mongoc_set_destroy (topology->rtt_monitors);
   topology->server_monitors = mongoc_set_new (1, NULL, NULL);
   topology->rtt_monitors = mongoc_set_new (1, NULL, NULL);
   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_OFF;
   mongoc_cond_broadcast (&topology->cond_client);
}

/* Cancel an in-progress streaming ismaster for a specific server (if
 * applicable).
 *
 * Called from application threads on network errors.
 * Caller must have topology mutex locked.
 */
void
_mongoc_topology_background_monitoring_cancel_check (
   mongoc_topology_t *topology, uint32_t server_id)
{
   mongoc_server_monitor_t *server_monitor;

   server_monitor = mongoc_set_get (topology->server_monitors, server_id);
   if (!server_monitor) {
      /* Already removed. */
      return;
   }
   mongoc_server_monitor_request_cancel (server_monitor);
}
