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
#include "mongoc-sdam-private.h"
#include "mongoc-trace.h"

#include "utlist.h"

void
_mongoc_sdam_background_thread_stop (mongoc_sdam_t *sdam);

bool
_mongoc_sdam_scanner_cb (uint32_t      id,
                         const bson_t *reply,
                         int64_t       rtt_msec,
                         void         *data,
                         bson_error_t *error)
{
   bool r;
   mongoc_sdam_t *sdam;
   mongoc_server_description_t *sd;
   mongoc_topology_description_type_t type;

   sdam = data;

   /* we're the only writer, so reading without the lock is fine */
   sd = _mongoc_topology_description_server_by_id (&sdam->topology, id);

   if (sd) {
      type = sdam->topology.type;

      /* hold the lock while we update */
      mongoc_mutex_lock (&sdam->mutex);
      r = _mongoc_topology_description_handle_ismaster (&sdam->topology, sd, reply, rtt_msec, error);

      if (type == sdam->topology.type) {
         /* wake up all clients if we found any topology changes */
         mongoc_cond_broadcast (&sdam->cond_client);
      }

      mongoc_mutex_unlock (&sdam->mutex);
   } else {
      r = false;
   }

   return r;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_new --
 *
 *       Creates and returns a new SDAM object.
 *
 *       NOTE: use _mongoc_sdam_grab() and _mongoc_sdam_release() to
 *       manage the lifetime of this object. Do not attempt to use this
 *       object before calling _mongoc_sdam_grab().
 *
 * Returns:
 *       A new SDAM object.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
mongoc_sdam_t *
_mongoc_sdam_new (const mongoc_uri_t *uri)
{
   mongoc_sdam_t *sdam;
   uint32_t id;
   const mongoc_host_list_t *hl;

   bson_return_val_if_fail(uri, NULL);

   sdam = bson_malloc0(sizeof *sdam);
   sdam->users = 0;
   sdam->uri = uri;

   /* TODO replace this */
   sdam->timeout_msec = 10000;

   _mongoc_topology_description_init(&sdam->topology, NULL);
   sdam->scanner = mongoc_sdam_scanner_new (_mongoc_sdam_scanner_cb, sdam);

   mongoc_mutex_init (&sdam->mutex);
   mongoc_cond_init (&sdam->cond_client);
   mongoc_cond_init (&sdam->cond_server);

   for ( hl = mongoc_uri_get_hosts (uri); hl; hl = hl->next) {
      id = _mongoc_topology_description_add_server (&sdam->topology, hl->host_and_port);
      mongoc_sdam_scanner_add (sdam->scanner, hl, id);
   }

   return sdam;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_grab --
 *
 *       Increments the users counter in @sdam.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_grab (mongoc_sdam_t *sdam)
{
   bson_return_if_fail(sdam);

   mongoc_mutex_lock (&sdam->mutex);
   sdam->users++;
   mongoc_mutex_unlock (&sdam->mutex);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_release --
 *
 *       Decrements number of users of @sdam.  If number falls to < 1,
 *       destroys this mongoc_sdam_t.  Treat this as destroy, and do not
 *       attempt to use @sdam after calling.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       May destroy @sdam.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_release (mongoc_sdam_t *sdam)
{
   bson_return_if_fail(sdam);

   mongoc_mutex_lock (&sdam->mutex);
   sdam->users--;
   if (sdam->users < 1) {
      mongoc_mutex_unlock (&sdam->mutex);
      _mongoc_sdam_destroy(sdam);
   } else {
      mongoc_mutex_unlock (&sdam->mutex);
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_destroy --
 *
 *       Free the memory associated with this sdam object.
 *
 *       NOTE: users should not call this directly; rather, use
 *       _mongoc_sdam_grab() and _mongoc_sdam_release() to indicate
 *       posession of this object.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @sdam will be cleaned up. Any users using this object without
 *       having called _mongoc_sdam_grab() will find themselves with
 *       a null pointer.
 *
 *-------------------------------------------------------------------------
 */
void
_mongoc_sdam_destroy (mongoc_sdam_t *sdam)
{
   _mongoc_sdam_background_thread_stop (sdam);
   _mongoc_topology_description_destroy(&sdam->topology);
   mongoc_sdam_scanner_destroy (sdam->scanner);
   mongoc_cond_destroy (&sdam->cond_client);
   mongoc_cond_destroy (&sdam->cond_server);
   mongoc_mutex_destroy (&sdam->mutex);
   bson_free(sdam);
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_select --
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
_mongoc_sdam_select (mongoc_sdam_t             *sdam,
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

   bson_return_val_if_fail(sdam, NULL);

   now = bson_get_monotonic_time ();
   expire_at = now + timeout_msec;

   mongoc_mutex_lock (&sdam->mutex);

   for (;;) {
      selected_server = _mongoc_topology_description_select(&sdam->topology,
                                                            optype,
                                                            read_prefs,
                                                            local_threshold_ms,
                                                            error);

      if (! selected_server) {
         r = mongoc_cond_timedwait (&sdam->cond_client, &sdam->mutex, expire_at - now);

         if (r == ETIMEDOUT) {
            mongoc_mutex_unlock (&sdam->mutex);

            /* handle timeouts */
            goto ERROR;
         } else if (r) {
            mongoc_mutex_unlock (&sdam->mutex);

            /* handle other errors */
            goto ERROR;
         }

         now = bson_get_monotonic_time ();
      } else {
         break;
      }
   }

   selected_server = _mongoc_server_description_new_copy(selected_server);
   mongoc_mutex_unlock (&sdam->mutex);

   return selected_server;

ERROR:

   return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_sdam_server_by_id --
 *
 *       Get the server description for @id, if that server is present
 *       in @description. Otherwise, return NULL.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
_mongoc_sdam_server_by_id (mongoc_sdam_t *sdam, uint32_t id)
{
   mongoc_server_description_t *sd;

   mongoc_mutex_lock (&sdam->mutex);

   sd = _mongoc_server_description_new_copy (
      _mongoc_topology_description_server_by_id (&sdam->topology, id));

   mongoc_mutex_unlock (&sdam->mutex);

   return sd;
}

void
_mongoc_sdam_start_scan (mongoc_sdam_t *sdam)
{
   int64_t now;
   bool do_scan = true;

   mongoc_mutex_lock (&sdam->mutex);

   if (sdam->scanning) {
      /* if we're already scanning, don't start a new one */
      do_scan = false;
   } else {
      now = bson_get_monotonic_time ();

      /* if we scanned too recently, just queue up the request for the
       * background thread */
      if (now - sdam->last_scan < MONGOC_SDAM_HEARTBEAT_FREQUENCY_MS) {
         sdam->scan_requested = true;
         do_scan = false;
      }
   }

   /* feel free to start the scan if none is currently in progress and it's
    * been long enough */
   if (do_scan) {
      mongoc_sdam_scanner_start_scan (sdam->scanner, sdam->timeout_msec);
   }

   /* This wakes up the background thread */
   mongoc_cond_signal (&sdam->cond_server);

   mongoc_mutex_unlock (&sdam->mutex);

   return;
}


/* not threadsafe, the expectation is that we're either single threaded or only
 * the background thread scans */
bool
_mongoc_sdam_scan (mongoc_sdam_t *sdam,
                   int64_t        work_msec)
{
   int64_t now;
   int64_t expire_at;
   bool keep_going = true;

   now = bson_get_monotonic_time ();
   expire_at = now + work_msec;

   /* while there is more work to do and we haven't timed out */
   while (keep_going && now < expire_at) {
      keep_going = mongoc_sdam_scanner_scan (sdam->scanner, expire_at - now);

      if (keep_going) {
         now = bson_get_monotonic_time ();
      }
   }

   return keep_going;
}


static
void * _mongoc_sdam_run_cb (void *data)
{
   mongoc_sdam_t *sdam;
   int64_t now;
   int64_t last_scan;
   int64_t timeout;
   int64_t force_timeout;
   int r;

   last_scan = 0;

   sdam = data;

   /* we exit htis loop when shutdown_requested, or on error */
   for (;;) {
      /* unlocked after starting a scan or after breaking out of the loop */
      mongoc_mutex_lock (&sdam->mutex);

      /* we exit this loop on error, or when we should scan immediately */
      for (;;) {
         if (sdam->shutdown_requested) goto DONE;

         now = bson_get_monotonic_time ();

         if (last_scan == 0) {
            /* set up the "last scan" as exactly long enough to force an
             * immediate scan on the first pass */

            last_scan = now - sdam->heartbeat_msec;
         }

         timeout = sdam->heartbeat_msec - (now - last_scan);

         /* if someone's specifically asked for a scan, use a shorter interval */
         if (sdam->scan_requested) {
            force_timeout = MONGOC_SDAM_MIN_HEARTBEAT_FREQUENCY_MS - (now - last_scan);

            timeout = MIN (timeout, force_timeout);
         }

         /* if we can start scanning, do so immediately */
         if (timeout <= 0) {
            mongoc_sdam_scanner_start_scan (sdam->scanner, sdam->timeout_msec);

            break;
         } else {
            /* otherwise wait until someone:
             *   o requests a scan
             *   o we time out
             *   o requests a shutdown
             */
            r = mongoc_cond_timedwait (&sdam->cond_server, &sdam->mutex, timeout);

            if (! (r == 0 || r == ETIMEDOUT)) {
               /* handle errors */
               goto DONE;
            }

            /* if we timed out, or were woken up, check if it's time to scan
             * again, or bail out */
         }
      }

      /* sdam_scan locks and unlocks the mutex itself until the scan is done */
      mongoc_mutex_unlock (&sdam->mutex);

      while (_mongoc_sdam_scan (sdam, sdam->timeout_msec)) {}
   }

DONE:

   mongoc_mutex_unlock (&sdam->mutex);

   return NULL;
}


void
_mongoc_sdam_background_thread_start (mongoc_sdam_t *sdam)
{
   bool launch_thread = true;

   mongoc_mutex_lock (&sdam->mutex);
   if (sdam->bg_thread_state != MONGOC_SDAM_BG_OFF) launch_thread = false;
   sdam->bg_thread_state = MONGOC_SDAM_BG_RUNNING;
   mongoc_mutex_unlock (&sdam->mutex);

   if (launch_thread) {
      mongoc_thread_create (&sdam->thread, _mongoc_sdam_run_cb, sdam);
   }
}


void
_mongoc_sdam_background_thread_stop (mongoc_sdam_t *sdam)
{
   bool join_thread = false;

   mongoc_mutex_lock (&sdam->mutex);
   if (sdam->bg_thread_state == MONGOC_SDAM_BG_RUNNING) {
      /* if the background thread is running, request a shutdown and signal the
       * thread */
      sdam->shutdown_requested = true;
      mongoc_cond_signal (&sdam->cond_server);
      sdam->bg_thread_state = MONGOC_SDAM_BG_SHUTTING_DOWN;
      join_thread = true;
   } else if (sdam->bg_thread_state == MONGOC_SDAM_BG_SHUTTING_DOWN) {
      /* if we're mid shutdown, wait until it shuts down */
      while (sdam->bg_thread_state != MONGOC_SDAM_BG_OFF) {
         mongoc_cond_wait (&sdam->cond_client, &sdam->mutex);
      }
   } else {
      /* nothing to do if it's already off */
   }

   mongoc_mutex_unlock (&sdam->mutex);

   if (join_thread) {
      /* if we're joining the thread, wait for it to come back and broadcast
       * all listeners */
      mongoc_thread_join (sdam->thread);

      mongoc_cond_broadcast (&sdam->cond_client);
   }
}


mongoc_sdam_bg_state_t
_mongoc_sdam_background_thread_state (mongoc_sdam_t *sdam)
{
   mongoc_sdam_bg_state_t state;

   mongoc_mutex_lock (&sdam->mutex);
   state = sdam->bg_thread_state;
   mongoc_mutex_unlock (&sdam->mutex);

   return state;
}
