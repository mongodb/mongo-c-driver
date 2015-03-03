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

#include "mongoc-error.h"
#include "mongoc-topology-private.h"
#include "mongoc-trace.h"

#include "utlist.h"

void
mongoc_topology_background_thread_stop (mongoc_topology_t *topology);

bool
_mongoc_topology_scanner_cb (uint32_t      id,
                             const bson_t *ismaster_response,
                             int64_t       rtt_msec,
                             void         *data,
                             bson_error_t *error)
{
   bool r = false;
   mongoc_topology_t *topology;
   mongoc_server_description_t *sd;

   bson_return_val_if_fail (data, false);

   topology = data;

   /* if multithreaded, driver may write through invalidate_server, so lock */
   mongoc_mutex_lock (&topology->mutex);

   sd = mongoc_topology_description_server_by_id (&topology->description, id);

   if (sd) {
      r = mongoc_topology_description_handle_ismaster (&topology->description, sd,
                                                       ismaster_response, rtt_msec,
                                                       error);
      /* TODO only wake up all clients if we found any topology changes */
      mongoc_cond_broadcast (&topology->cond_client);
   }

   mongoc_mutex_unlock (&topology->mutex);

   return r;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_new --
 *
 *       Creates and returns a new topology object.
 *
 * Returns:
 *       A new topology object.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
mongoc_topology_t *
mongoc_topology_new (const mongoc_uri_t *uri)
{
   mongoc_topology_t *topology;
   mongoc_topology_description_type_t init_type;
   uint32_t id;
   const mongoc_host_list_t *hl;

   bson_return_val_if_fail(uri, NULL);

   topology = bson_malloc0(sizeof *topology);

   /*
    * Not ideal, but there's no great way to do this.
    * Base on the URI, we assume:
    *   - if we've got a replicaSet name, initialize to RS_NO_PRIMARY
    *   - otherwise, if the seed list has a single host, initialize to SINGLE
    *   - everything else gets initialized to UNKNOWN
    */
   if (mongoc_uri_get_replica_set(uri)) {
      init_type = MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else {
      hl = mongoc_uri_get_hosts(uri);
      if (hl->next) {
         init_type = MONGOC_TOPOLOGY_UNKNOWN;
      } else {
         init_type = MONGOC_TOPOLOGY_SINGLE;
      }
   }

   mongoc_topology_description_init(&topology->description, init_type, NULL);
   topology->description.set_name = bson_strdup(mongoc_uri_get_replica_set(uri));

   topology->scanner = mongoc_topology_scanner_new (_mongoc_topology_scanner_cb,
                                                    topology);
   topology->uri = uri;
   topology->single_threaded = true;

   /* TODO make SS timeout configurable on client */
   topology->timeout_msec = 30000;

   mongoc_mutex_init (&topology->mutex);
   mongoc_cond_init (&topology->cond_client);
   mongoc_cond_init (&topology->cond_server);

   for ( hl = mongoc_uri_get_hosts (uri); hl; hl = hl->next) {
      mongoc_topology_description_add_server (&topology->description,
                                              hl->host_and_port,
                                              &id);
      mongoc_topology_scanner_add (topology->scanner, hl, id);
   }

   return topology;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_destroy --
 *
 *       Free the memory associated with this topology object.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @topology will be cleaned up.
 *
 *-------------------------------------------------------------------------
 */
void
mongoc_topology_destroy (mongoc_topology_t *topology)
{
   if (!topology) {
      return;
   }

   mongoc_topology_description_destroy(&topology->description);
   mongoc_topology_scanner_destroy (topology->scanner);
   mongoc_cond_destroy (&topology->cond_client);
   mongoc_cond_destroy (&topology->cond_server);
   mongoc_mutex_destroy (&topology->mutex);

   bson_free(topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_time_to_scan --
 *
 *       Returns true if it is time to scan the cluster again.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_time_to_scan (mongoc_topology_t *topology) {
   return (bson_get_monotonic_time() - topology->last_scan >= MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_run_scanner --
 *
 *       Not threadsafe, the expectation is that we're either single
 *       threaded or only the background thread runs scans.
 *
 *       Crank the underlying scanner until we've timed out or finished.
 *
 * Returns:
 *       true if there is more work to do, false otherwise
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_run_scanner (mongoc_topology_t *topology,
                              int64_t        work_msec)
{
   int64_t now;
   int64_t expire_at;
   bool keep_going = true;

   now = bson_get_monotonic_time ();
   expire_at = now + work_msec;

   /* while there is more work to do and we haven't timed out */
   while (keep_going && now < expire_at) {
      keep_going = mongoc_topology_scanner_work (topology->scanner, expire_at - now);

      if (keep_going) {
         now = bson_get_monotonic_time ();
      }
   }

   return keep_going;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_do_blocking_scan --
 *
 *       Monitoring entry for single-threaded use case. Assumes the caller
 *       has checked that it's the right time to scan.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_do_blocking_scan (mongoc_topology_t *topology) {
   mongoc_topology_scanner_start (topology->scanner, topology->timeout_msec);
   while (_mongoc_topology_run_scanner (topology, topology->timeout_msec)) {}
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_select --
 *
 *       Selects a server description for an operation based on @optype
 *       and @read_prefs.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL on failure, in which case
 *       @error will be set.
 *
 * Side effects:
 *       @error may be set.
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
mongoc_topology_select (mongoc_topology_t         *topology,
                        mongoc_ss_optype_t         optype,
                        const mongoc_read_prefs_t *read_prefs,
                        int64_t                    timeout_msec,
                        int64_t                    local_threshold_ms,
                        bson_error_t              *error)
{
   int r;
   int64_t now;
   int64_t expire_at;
   mongoc_server_description_t *selected_server = NULL;

   bson_return_val_if_fail(topology, NULL);

   now = bson_get_monotonic_time ();
   expire_at = now + timeout_msec;

   /* run single-threaded algorithm if we must */
   if (topology->single_threaded) {

      /* if enough time has passed or we're stale, block and scan */
      if (_mongoc_topology_time_to_scan(topology) || topology->stale) {
         _mongoc_topology_do_blocking_scan(topology);
         topology->stale = false;
      }

      /* until we find a server or timeout */
      for (;;) {
         /* attempt to select a server */
         selected_server = mongoc_topology_description_select(&topology->description,
                                                              optype,
                                                              read_prefs,
                                                              local_threshold_ms,
                                                              error);

         if (selected_server) {
            return mongoc_server_description_new_copy(selected_server);
         }

         /* rescan */
         _mongoc_topology_do_blocking_scan(topology);

         /* error if we've timed out */
         now = bson_get_monotonic_time();
         if (now > expire_at) {
            goto TIMEOUT;
         }
      }
   }

   /* With background thread */
   /* we break out when we've found a server or timed out */
   for (;;) {
      mongoc_mutex_lock (&topology->mutex);
      selected_server = mongoc_topology_description_select(&topology->description,
                                                           optype,
                                                           read_prefs,
                                                           local_threshold_ms,
                                                           error);

      if (! selected_server) {
         /* TODO request an immediate topology check here */
         r = mongoc_cond_timedwait (&topology->cond_client, &topology->mutex, expire_at - now);
         mongoc_mutex_unlock (&topology->mutex);

         if (r == ETIMEDOUT) {
            /* handle timeouts */
            goto TIMEOUT;
         } else if (r) {
            /* TODO handle other errors */
            goto TIMEOUT;
         }

         now = bson_get_monotonic_time ();
      } else {
         selected_server = mongoc_server_description_new_copy(selected_server);
         mongoc_mutex_unlock (&topology->mutex);
         return selected_server;
      }
   }

 TIMEOUT:
   bson_set_error(error,
                  MONGOC_ERROR_SERVER_SELECTION,
                  MONGOC_ERROR_SERVER_SELECTION_TIMEOUT,
                  "Timed out trying to select a server");
   return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_server_by_id --
 *
 *      Get the server description for @id, if that server is present
 *      in @description. Otherwise, return NULL.
 *
 *      NOTE: this method returns a copy of the original server
 *      description. Callers must own and clean up this copy.
 *
 * Returns:
 *      A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
mongoc_topology_server_by_id (mongoc_topology_t *topology, uint32_t id)
{
   mongoc_server_description_t *sd;

   mongoc_mutex_lock (&topology->mutex);

   sd = mongoc_server_description_new_copy (
      mongoc_topology_description_server_by_id (&topology->description, id));

   mongoc_mutex_unlock (&topology->mutex);

   return sd;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_request_scan --
 *
 *       Used from within the driver to request an immediate topology check.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_topology_request_scan (mongoc_topology_t *topology)
{
   bool do_scan = true;

   bson_return_if_fail (topology);

   mongoc_mutex_lock (&topology->mutex);

   if (topology->scanning) {
      /* if we're already scanning, don't start a new one */
      do_scan = false;
   } else {
      /* if we scanned too recently, just queue up the request for the
       * background thread */
      if (_mongoc_topology_time_to_scan(topology)) {
         topology->scan_requested = true;
         do_scan = false;
      }
   }

   /* feel free to start the scan if none is currently in progress and it's
    * been long enough */
   if (do_scan) {
      mongoc_topology_scanner_start (topology->scanner, topology->timeout_msec);
   }

   mongoc_mutex_unlock (&topology->mutex);

   /* This wakes up the background thread */
   mongoc_cond_signal (&topology->cond_server);

   return;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_invalidate_server --
 *
 *      Invalidate the given server after receiving a network error in
 *      another part of the client.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_topology_invalidate_server (mongoc_topology_t *topology,
                                   uint32_t           id)
{
   mongoc_mutex_lock (&topology->mutex);
   mongoc_topology_description_invalidate_server (&topology->description, id);
   mongoc_mutex_unlock (&topology->mutex);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_run_background --
 *
 *       The background topology monitoring thread runs in this loop.
 *
 *--------------------------------------------------------------------------
 */
static
void * _mongoc_topology_run_background (void *data)
{
   mongoc_topology_t *topology;
   int64_t now;
   int64_t last_scan;
   int64_t timeout;
   int64_t force_timeout;
   int r;

   bson_return_val_if_fail (data, NULL);

   last_scan = 0;
   topology = data;
   /* we exit this loop when shutdown_requested, or on error */
   for (;;) {
      /* unlocked after starting a scan or after breaking out of the loop */
      mongoc_mutex_lock (&topology->mutex);

      /* we exit this loop on error, or when we should scan immediately */
      for (;;) {
         if (topology->shutdown_requested) goto DONE;

         now = bson_get_monotonic_time ();

         if (last_scan == 0) {
            /* set up the "last scan" as exactly long enough to force an
             * immediate scan on the first pass */
            last_scan = now - topology->heartbeat_msec;
         }

         timeout = topology->heartbeat_msec - (now - last_scan);

         /* if someone's specifically asked for a scan, use a shorter interval */
         if (topology->scan_requested) {
            force_timeout = MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS - (now - last_scan);

            timeout = BSON_MIN (timeout, force_timeout);
         }

         /* if we can start scanning, do so immediately */
         if (timeout <= 0) {
            mongoc_topology_scanner_start (topology->scanner, topology->timeout_msec);
            break;
         } else {
            /* otherwise wait until someone:
             *   o requests a scan
             *   o we time out
             *   o requests a shutdown
             */
            r = mongoc_cond_timedwait (&topology->cond_server, &topology->mutex, timeout);
            if (! (r == 0 || r == ETIMEDOUT)) {
               /* handle errors */
               goto DONE;
            }

            /* if we timed out, or were woken up, check if it's time to scan
             * again, or bail out */
         }
      }

      /* scanning locks and unlocks the mutex itself until the scan is done */
      mongoc_mutex_unlock (&topology->mutex);

      while (_mongoc_topology_run_scanner (topology, topology->timeout_msec)) {}
   }

DONE:
   mongoc_mutex_unlock (&topology->mutex);

   return NULL;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_background_thread_start --
 *
 *       Start the topology background thread running. This should only be
 *       called once per pool. If clients are created separately (not
 *       through a pool) the SDAM logic will not be run in a background
 *       thread.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_background_thread_start (mongoc_topology_t *topology)
{
   bool launch_thread = true;

   if (topology->single_threaded) {
      return;
   }

   mongoc_mutex_lock (&topology->mutex);
   if (topology->bg_thread_state != MONGOC_TOPOLOGY_BG_OFF) launch_thread = false;
   topology->bg_thread_state = MONGOC_TOPOLOGY_BG_RUNNING;
   mongoc_mutex_unlock (&topology->mutex);

   if (launch_thread) {
      mongoc_thread_create (&topology->thread, _mongoc_topology_run_background,
                            topology);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_background_thread_stop --
 *
 *       Stop the topology background thread. Called by the owning pool at
 *       its destruction.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_background_thread_stop (mongoc_topology_t *topology)
{
   bool join_thread = false;

   if (topology->single_threaded) {
      return;
   }

   mongoc_mutex_lock (&topology->mutex);
   if (topology->bg_thread_state == MONGOC_TOPOLOGY_BG_RUNNING) {
      /* if the background thread is running, request a shutdown and signal the
       * thread */
      topology->shutdown_requested = true;
      mongoc_cond_signal (&topology->cond_server);
      topology->bg_thread_state = MONGOC_TOPOLOGY_BG_SHUTTING_DOWN;
      join_thread = true;
   } else if (topology->bg_thread_state == MONGOC_TOPOLOGY_BG_SHUTTING_DOWN) {
      /* if we're mid shutdown, wait until it shuts down */
      while (topology->bg_thread_state != MONGOC_TOPOLOGY_BG_OFF) {
         mongoc_cond_wait (&topology->cond_client, &topology->mutex);
      }
   } else {
      /* nothing to do if it's already off */
   }

   mongoc_mutex_unlock (&topology->mutex);

   if (join_thread) {
      /* if we're joining the thread, wait for it to come back and broadcast
       * all listeners */
      mongoc_thread_join (topology->thread);
      mongoc_cond_broadcast (&topology->cond_client);
   }
}
