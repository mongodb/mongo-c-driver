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

#include "mongoc-config.h"

#include "mongoc-handshake.h"
#include "mongoc-handshake-private.h"

#include "mongoc-error.h"
#include "mongoc-host-list-private.h"
#include "mongoc-log.h"
#include "mongoc-topology-private.h"
#include "mongoc-topology-description-apm-private.h"
#include "mongoc-client-private.h"
#include "mongoc-cmd-private.h"
#include "mongoc-uri-private.h"
#include "mongoc-util-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-error-private.h"
#include "mongoc-topology-background-monitoring-private.h"
#include "mongoc-read-prefs-private.h"

#include "utlist.h"

static void
_topology_collect_errors (mongoc_topology_t *topology, bson_error_t *error_out);

static bool
_mongoc_topology_reconcile_add_nodes (mongoc_server_description_t *sd,
                                      mongoc_topology_t *topology)
{
   mongoc_topology_scanner_t *scanner = topology->scanner;
   mongoc_topology_scanner_node_t *node;

   /* Search by ID and update hello_ok */
   node = mongoc_topology_scanner_get_node (scanner, sd->id);
   if (node) {
      node->hello_ok = sd->hello_ok;
   } else if (!mongoc_topology_scanner_has_node_for_host (scanner, &sd->host)) {
      /* A node for this host was retired in this scan. */
      mongoc_topology_scanner_add (scanner, &sd->host, sd->id, sd->hello_ok);
      mongoc_topology_scanner_scan (scanner, sd->id);
   }

   return true;
}

/* Called from:
 * - the topology scanner callback (when a hello was just received)
 * - at the start of a single-threaded scan (mongoc_topology_scan_once)
 * Not called for multi threaded monitoring.
 */
void
mongoc_topology_reconcile (mongoc_topology_t *topology)
{
   mongoc_topology_description_t *description;
   mongoc_set_t *servers;
   mongoc_server_description_t *sd;
   int i;
   mongoc_topology_scanner_node_t *ele, *tmp;

   description = &topology->description;
   servers = description->servers;

   /* Add newly discovered nodes */
   for (i = 0; i < (int) servers->items_len; i++) {
      sd = (mongoc_server_description_t *) mongoc_set_get_item (servers, i);
      _mongoc_topology_reconcile_add_nodes (sd, topology);
   }

   /* Remove removed nodes */
   DL_FOREACH_SAFE (topology->scanner->nodes, ele, tmp)
   {
      if (!mongoc_topology_description_server_by_id (
             description, ele->id, NULL)) {
         mongoc_topology_scanner_node_retire (ele);
      }
   }
}


/* call this while already holding the lock */
static bool
_mongoc_topology_update_no_lock (uint32_t id,
                                 const bson_t *hello_response,
                                 int64_t rtt_msec,
                                 mongoc_topology_t *topology,
                                 const bson_error_t *error /* IN */)
{
   mongoc_topology_description_handle_hello (
      &topology->description, id, hello_response, rtt_msec, error);

   /* return false if server removed from topology */
   return mongoc_topology_description_server_by_id (
             &topology->description, id, NULL) != NULL;
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_scanner_setup_err_cb --
 *
 *       Callback method to handle errors during topology scanner node
 *       setup, typically DNS or SSL errors.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_topology_scanner_setup_err_cb (uint32_t id,
                                       void *data,
                                       const bson_error_t *error /* IN */)
{
   mongoc_topology_t *topology;

   BSON_ASSERT (data);

   topology = (mongoc_topology_t *) data;

   if (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED) {
      /* In load balanced mode, scanning is only for connection establishment.
       * It must not modify the topology description. */
      return;
   }

   mongoc_topology_description_handle_hello (&topology->description,
                                             id,
                                             NULL /* hello reply */,
                                             -1 /* rtt_msec */,
                                             error);
}


/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_scanner_cb --
 *
 *       Callback method to handle hello responses received by async
 *       command objects.
 *
 *       NOTE: This method locks the given topology's mutex.
 *       Only called for single-threaded monitoring.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_topology_scanner_cb (uint32_t id,
                             const bson_t *hello_response,
                             int64_t rtt_msec,
                             void *data,
                             const bson_error_t *error /* IN */)
{
   mongoc_topology_t *topology;
   mongoc_server_description_t *sd;

   BSON_ASSERT (data);

   topology = (mongoc_topology_t *) data;

   if (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED) {
      /* In load balanced mode, scanning is only for connection establishment.
       * It must not modify the topology description. */
      return;
   }

   bson_mutex_lock (&topology->mutex);
   sd = mongoc_topology_description_server_by_id (
      &topology->description, id, NULL);

   if (!hello_response) {
      /* Server monitoring: When a server check fails due to a network error
       * (including a network timeout), the client MUST clear its connection
       * pool for the server */
      _mongoc_topology_clear_connection_pool (topology, id, &kZeroServiceId);
   }

   /* Server Discovery and Monitoring Spec: "Once a server is connected, the
    * client MUST change its type to Unknown only after it has retried the
    * server once." */
   if (!hello_response && sd && sd->type != MONGOC_SERVER_UNKNOWN) {
      _mongoc_topology_update_no_lock (
         id, hello_response, rtt_msec, topology, error);

      /* add another hello call to the current scan - the scan continues
       * until all commands are done */
      mongoc_topology_scanner_scan (topology->scanner, sd->id);
   } else {
      _mongoc_topology_update_no_lock (
         id, hello_response, rtt_msec, topology, error);

      /* The processing of the hello results above may have added, changed, or
       * removed server descriptions. We need to reconcile that with our
       * monitoring agents
       */
      mongoc_topology_reconcile (topology);

      mongoc_cond_broadcast (&topology->cond_client);
   }

   bson_mutex_unlock (&topology->mutex);
}

static void
_server_session_init (mongoc_server_session_t *session,
                      mongoc_topology_t *unused,
                      bson_error_t *error)
{
   _mongoc_server_session_init (session, error);
}

static void
_server_session_destroy (mongoc_server_session_t *session,
                         mongoc_topology_t *unused)
{
   _mongoc_server_session_destroy (session);
}

static int
_server_session_should_prune (mongoc_server_session_t *session,
                              mongoc_topology_t *topo)
{
   bool is_loadbalanced;
   int timeout;
   BSON_ASSERT_PARAM (session);
   BSON_ASSERT_PARAM (topo);

   /** If "dirty" (i.e. contains a network error), it should be dropped */
   if (session->dirty) {
      return true;
   }

   /** If the session has never been used, it should be dropped */
   if (session->last_used_usec == SESSION_NEVER_USED) {
      return true;
   }

   /* Check for a timeout */
   bson_mutex_lock (&topo->mutex);
   timeout = topo->description.session_timeout_minutes;
   is_loadbalanced = topo->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED;
   bson_mutex_unlock (&topo->mutex);

   /** Load balanced topology sessions never expire */
   if (is_loadbalanced) {
      return false;
   }

   /* Prune the session if it has hit a timeout */
   return _mongoc_server_session_timed_out (session, timeout);
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
mongoc_topology_new (const mongoc_uri_t *uri, bool single_threaded)
{
   int64_t heartbeat_default;
   int64_t heartbeat;
   mongoc_topology_t *topology;
   bool topology_valid;
   mongoc_topology_description_type_t init_type;
   const char *service;
   char *prefixed_service;
   uint32_t id;
   const mongoc_host_list_t *hl;
   mongoc_rr_data_t rr_data;
   bool has_directconnection;
   bool directconnection;

   BSON_ASSERT (uri);
   topology_valid = false;

#ifndef MONGOC_ENABLE_CRYPTO
   if (mongoc_uri_get_option_as_bool (
          uri, MONGOC_URI_RETRYWRITES, MONGOC_DEFAULT_RETRYWRITES)) {
      /* retryWrites requires sessions, which require crypto - just warn */
      MONGOC_WARNING (
         "retryWrites not supported without an SSL crypto library");
   }
#endif

   topology = (mongoc_topology_t *) bson_malloc0 (sizeof *topology);
   topology->session_pool =
      mongoc_server_session_pool_new_with_params (_server_session_init,
                                                  _server_session_destroy,
                                                  _server_session_should_prune,
                                                  topology);
   heartbeat_default =
      single_threaded ? MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED
                      : MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_MULTI_THREADED;

   heartbeat = mongoc_uri_get_option_as_int32 (
      uri, MONGOC_URI_HEARTBEATFREQUENCYMS, heartbeat_default);

   mongoc_topology_description_init (&topology->description, heartbeat);

   topology->description.set_name =
      bson_strdup (mongoc_uri_get_replica_set (uri));

   topology->uri = mongoc_uri_copy (uri);

   topology->single_threaded = single_threaded;
   if (single_threaded) {
      /* Server Selection Spec:
       *
       *   "Single-threaded drivers MUST provide a "serverSelectionTryOnce"
       *   mode, in which the driver scans the topology exactly once after
       *   server selection fails, then either selects a server or raises an
       *   error.
       *
       *   "The serverSelectionTryOnce option MUST be true by default."
       */
      topology->server_selection_try_once = mongoc_uri_get_option_as_bool (
         uri, MONGOC_URI_SERVERSELECTIONTRYONCE, true);
   } else {
      topology->server_selection_try_once = false;
   }

   topology->server_selection_timeout_msec = mongoc_uri_get_option_as_int32 (
      topology->uri,
      MONGOC_URI_SERVERSELECTIONTIMEOUTMS,
      MONGOC_TOPOLOGY_SERVER_SELECTION_TIMEOUT_MS);

   /* tests can override this */
   topology->min_heartbeat_frequency_msec =
      MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS;

   topology->local_threshold_msec =
      mongoc_uri_get_local_threshold_option (topology->uri);

   /* Total time allowed to check a server is connectTimeoutMS.
    * Server Discovery And Monitoring Spec:
    *
    *   "The socket used to check a server MUST use the same connectTimeoutMS as
    *   regular sockets. Multi-threaded clients SHOULD set monitoring sockets'
    *   socketTimeoutMS to the connectTimeoutMS."
    */
   topology->connect_timeout_msec =
      mongoc_uri_get_option_as_int32 (topology->uri,
                                      MONGOC_URI_CONNECTTIMEOUTMS,
                                      MONGOC_DEFAULT_CONNECTTIMEOUTMS);

   topology->scanner_state = MONGOC_TOPOLOGY_SCANNER_OFF;
   topology->scanner =
      mongoc_topology_scanner_new (topology->uri,
                                   _mongoc_topology_scanner_setup_err_cb,
                                   _mongoc_topology_scanner_cb,
                                   topology,
                                   topology->connect_timeout_msec);

   bson_mutex_init (&topology->mutex);
   mongoc_cond_init (&topology->cond_client);

   if (single_threaded) {
      /* single threaded drivers attempt speculative authentication during a
       * topology scan */
      topology->scanner->speculative_authentication = true;

      /* single threaded clients negotiate sasl supported mechanisms during
       * a topology scan. */
      if (_mongoc_uri_requires_auth_negotiation (uri)) {
         topology->scanner->negotiate_sasl_supported_mechs = true;
      }
   }

   service = mongoc_uri_get_service (uri);
   if (service) {
      memset (&rr_data, 0, sizeof (mongoc_rr_data_t));
      /* Set the default resource record resolver */
      topology->rr_resolver = _mongoc_client_get_rr;

      /* Initialize the last scan time and interval. Even if the initial DNS
       * lookup fails, SRV polling will still start when background monitoring
       * starts. */
      topology->srv_polling_last_scan_ms = bson_get_monotonic_time () / 1000;
      topology->srv_polling_rescan_interval_ms =
         MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS;

      /* a mongodb+srv URI. try SRV lookup, if no error then also try TXT */
      prefixed_service = bson_strdup_printf ("_mongodb._tcp.%s", service);
      if (!topology->rr_resolver (prefixed_service,
                                  MONGOC_RR_SRV,
                                  &rr_data,
                                  MONGOC_RR_DEFAULT_BUFFER_SIZE,
                                  &topology->scanner->error)) {
         GOTO (srv_fail);
      }

      /* Failure to find TXT records will not return an error (since it is only
       * for options). But _mongoc_client_get_rr may return an error if
       * there is more than one TXT record returned. */
      if (!topology->rr_resolver (service,
                                  MONGOC_RR_TXT,
                                  &rr_data,
                                  MONGOC_RR_DEFAULT_BUFFER_SIZE,
                                  &topology->scanner->error)) {
         GOTO (srv_fail);
      }

      /* Use rr_data to update the topology's URI. */
      if (rr_data.txt_record_opts &&
          !mongoc_uri_parse_options (topology->uri,
                                     rr_data.txt_record_opts,
                                     true /* from_dns */,
                                     &topology->scanner->error)) {
         GOTO (srv_fail);
      }

      if (!mongoc_uri_init_with_srv_host_list (
             topology->uri, rr_data.hosts, &topology->scanner->error)) {
         GOTO (srv_fail);
      }

      topology->srv_polling_last_scan_ms = bson_get_monotonic_time () / 1000;
      /* TODO (CDRIVER-4047) use BSON_MIN */
      topology->srv_polling_rescan_interval_ms = BSON_MAX (
         rr_data.min_ttl * 1000, MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS);

      topology_valid = true;
   srv_fail:
      bson_free (rr_data.txt_record_opts);
      bson_free (prefixed_service);
      _mongoc_host_list_destroy_all (rr_data.hosts);
   } else {
      topology_valid = true;
   }

   if (!mongoc_uri_finalize_loadbalanced (topology->uri,
                                          &topology->scanner->error)) {
      topology_valid = false;
   }

   /*
    * Set topology type from URI:
    *   + if directConnection=true
    *     - whether or not we have a replicaSet name, initialize to SINGLE
    *     (directConnect with SRV or multiple hosts triggers a URI parse error)
    *   + if directConnection=false
    *     - if we've got a replicaSet name, initialize to RS_NO_PRIMARY
    *     - otherwise, initialize to UNKNOWN
    *   + if directConnection was not specified in the URI (old behavior)
    *     - if we've got a replicaSet name, initialize to RS_NO_PRIMARY
    *     - otherwise, if the seed list has a single host, initialize to SINGLE
    *   - everything else gets initialized to UNKNOWN
    */
   has_directconnection =
      mongoc_uri_has_option (uri, MONGOC_URI_DIRECTCONNECTION);
   directconnection =
      has_directconnection &&
      mongoc_uri_get_option_as_bool (uri, MONGOC_URI_DIRECTCONNECTION, false);
   hl = mongoc_uri_get_hosts (topology->uri);
   /* If loadBalanced is enabled, directConnection is disabled. This was
    * validated in mongoc_uri_finalize_loadbalanced. */
   if (mongoc_uri_get_option_as_bool (
          topology->uri, MONGOC_URI_LOADBALANCED, false)) {
      init_type = MONGOC_TOPOLOGY_LOAD_BALANCED;
      if (topology->single_threaded) {
         /* Cooldown only applies to server monitoring for single-threaded
          * clients. In load balanced mode, the topology scanner is used to
          * create connections. The cooldown period does not apply. A network
          * error to a load balanced connection does not imply subsequent
          * connection attempts will be to the same server and that a delay
          * should occur. */
         _mongoc_topology_bypass_cooldown (topology);
      }
      _mongoc_topology_scanner_set_loadbalanced (topology->scanner, true);
   } else if (service && !has_directconnection) {
      init_type = MONGOC_TOPOLOGY_UNKNOWN;
   } else if (has_directconnection) {
      if (directconnection) {
         init_type = MONGOC_TOPOLOGY_SINGLE;
      } else {
         if (mongoc_uri_get_replica_set (topology->uri)) {
            init_type = MONGOC_TOPOLOGY_RS_NO_PRIMARY;
         } else {
            init_type = MONGOC_TOPOLOGY_UNKNOWN;
         }
      }
   } else if (mongoc_uri_get_replica_set (topology->uri)) {
      init_type = MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else {
      if (hl && hl->next) {
         init_type = MONGOC_TOPOLOGY_UNKNOWN;
      } else {
         init_type = MONGOC_TOPOLOGY_SINGLE;
      }
   }

   topology->description.type = init_type;

   if (!topology->single_threaded) {
      topology->server_monitors = mongoc_set_new (1, NULL, NULL);
      topology->rtt_monitors = mongoc_set_new (1, NULL, NULL);
      bson_mutex_init (&topology->apm_mutex);
      mongoc_cond_init (&topology->srv_polling_cond);
   }

   if (!topology_valid) {
      TRACE ("%s", "topology invalid");
      /* add no nodes */
      return topology;
   }

   while (hl) {
      mongoc_topology_description_add_server (
         &topology->description, hl->host_and_port, &id);
      mongoc_topology_scanner_add (topology->scanner, hl, id, false);

      hl = hl->next;
   }

   return topology;
}
/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_set_apm_callbacks --
 *
 *       Set Application Performance Monitoring callbacks.
 *
 * Caller must hold topology->mutex.
 *
 *-------------------------------------------------------------------------
 */
void
mongoc_topology_set_apm_callbacks (mongoc_topology_t *topology,
                                   mongoc_apm_callbacks_t *callbacks,
                                   void *context)
{
   if (callbacks) {
      memcpy (&topology->description.apm_callbacks,
              callbacks,
              sizeof (mongoc_apm_callbacks_t));
      memcpy (&topology->scanner->apm_callbacks,
              callbacks,
              sizeof (mongoc_apm_callbacks_t));
   } else {
      memset (&topology->description.apm_callbacks,
              0,
              sizeof (mongoc_apm_callbacks_t));
      memset (
         &topology->scanner->apm_callbacks, 0, sizeof (mongoc_apm_callbacks_t));
   }

   topology->description.apm_context = context;
   topology->scanner->apm_context = context;
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

#ifdef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION
   bson_free (topology->keyvault_db);
   bson_free (topology->keyvault_coll);
   mongoc_client_destroy (topology->mongocryptd_client);
   mongoc_client_pool_destroy (topology->mongocryptd_client_pool);
   _mongoc_crypt_destroy (topology->crypt);
   bson_destroy (topology->mongocryptd_spawn_args);
   bson_free (topology->mongocryptd_spawn_path);
#endif

   if (!topology->single_threaded) {
      bson_mutex_lock (&topology->mutex);
      _mongoc_topology_background_monitoring_stop (topology);
      bson_mutex_unlock (&topology->mutex);
      BSON_ASSERT (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_OFF);
      mongoc_set_destroy (topology->server_monitors);
      mongoc_set_destroy (topology->rtt_monitors);
      bson_mutex_destroy (&topology->apm_mutex);
      mongoc_cond_destroy (&topology->srv_polling_cond);
   }
   _mongoc_topology_description_monitor_closed (&topology->description);

   mongoc_uri_destroy (topology->uri);
   mongoc_topology_description_cleanup (&topology->description);
   mongoc_topology_scanner_destroy (topology->scanner);
   mongoc_server_session_pool_free (topology->session_pool);

   mongoc_cond_destroy (&topology->cond_client);
   bson_mutex_destroy (&topology->mutex);

   bson_free (topology);
}

/* Returns false if none of the hosts were valid. */
bool
mongoc_topology_apply_scanned_srv_hosts (mongoc_uri_t *uri,
                                         mongoc_topology_description_t *td,
                                         mongoc_host_list_t *hosts,
                                         bson_error_t *error)
{
   mongoc_host_list_t *host;
   mongoc_host_list_t *valid_hosts = NULL;
   bool had_valid_hosts = false;

   /* Validate that the hosts have a matching domain.
    * If validation fails, log it.
    * If no valid hosts remain, do not update the topology description.
    */
   LL_FOREACH (hosts, host)
   {
      if (mongoc_uri_validate_srv_result (uri, host->host, error)) {
         _mongoc_host_list_upsert (&valid_hosts, host);
      } else {
         MONGOC_ERROR ("Invalid host returned by SRV: %s", host->host_and_port);
         /* Continue on, there may still be valid hosts returned. */
      }
   }

   if (valid_hosts) {
      /* Reconcile with the topology description. Newly found servers will start
       * getting monitored and are eligible to be used by clients. */
      mongoc_topology_description_reconcile (td, valid_hosts);
      had_valid_hosts = true;
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                      "SRV response did not contain any valid hosts");
   }

   _mongoc_host_list_destroy_all (valid_hosts);
   return had_valid_hosts;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_should_rescan_srv --
 *
 *      Checks whether it is valid to rescan SRV records on the topology.
 *      Namely, that the topology type is Sharded or Unknown, and that
 *      the topology URI was configured with SRV.
 *
 *      If this returns false, caller can stop scanning SRV records
 *      and does not need to try again in the future.
 *
 *      NOTE: this method expects @topology's mutex to be locked on entry.
 *
 * --------------------------------------------------------------------------
 */
bool
mongoc_topology_should_rescan_srv (mongoc_topology_t *topology)
{
   const char *service;

   MONGOC_DEBUG_ASSERT (COMMON_PREFIX (mutex_is_locked) (&topology->mutex));

   service = mongoc_uri_get_service (topology->uri);
   if (!service) {
      /* Only rescan if we have a mongodb+srv:// URI. */
      return false;
   }


   if ((topology->description.type != MONGOC_TOPOLOGY_SHARDED) &&
       (topology->description.type != MONGOC_TOPOLOGY_UNKNOWN)) {
      /* Only perform rescan for sharded topology. */
      return false;
   }

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_rescan_srv --
 *
 *      Queries SRV records for new hosts in a mongos cluster.
 *      Caller must call mongoc_topology_should_rescan_srv before calling
 *      to ensure preconditions are met (while holding @topology's mutex
 *      for the duration of both calls).
 *
 *      NOTE: this method expects @topology's mutex to be locked on entry.
 *
 * --------------------------------------------------------------------------
 */
void
mongoc_topology_rescan_srv (mongoc_topology_t *topology)
{
   mongoc_rr_data_t rr_data = {0};
   const char *service;
   char *prefixed_service = NULL;
   int64_t scan_time_ms;
   bool ret;

   MONGOC_DEBUG_ASSERT (COMMON_PREFIX (mutex_is_locked) (&topology->mutex));
   BSON_ASSERT (mongoc_topology_should_rescan_srv (topology));

   service = mongoc_uri_get_service (topology->uri);
   scan_time_ms = topology->srv_polling_last_scan_ms +
                  topology->srv_polling_rescan_interval_ms;
   if (bson_get_monotonic_time () / 1000 < scan_time_ms) {
      /* Query SRV no more frequently than srv_polling_rescan_interval_ms. */
      return;
   }

   TRACE ("%s", "Polling for SRV records");

   /* Go forth and query... */
   prefixed_service = bson_strdup_printf ("_mongodb._tcp.%s", service);

   /* Unlock topology mutex during scan so it does not hold up other operations.
    */
   bson_mutex_unlock (&topology->mutex);
   ret = topology->rr_resolver (prefixed_service,
                                MONGOC_RR_SRV,
                                &rr_data,
                                MONGOC_RR_DEFAULT_BUFFER_SIZE,
                                &topology->scanner->error);
   bson_mutex_lock (&topology->mutex);

   topology->srv_polling_last_scan_ms = bson_get_monotonic_time () / 1000;
   if (!ret) {
      /* Failed querying, soldier on and try again next time. */
      topology->srv_polling_rescan_interval_ms =
         topology->description.heartbeat_msec;
      MONGOC_ERROR ("SRV polling error: %s", topology->scanner->error.message);
      GOTO (done);
   }

   /* TODO (CDRIVER-4047) use BSON_MIN */
   topology->srv_polling_rescan_interval_ms = BSON_MAX (
      rr_data.min_ttl * 1000, MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS);

   if (!mongoc_topology_apply_scanned_srv_hosts (topology->uri,
                                                 &topology->description,
                                                 rr_data.hosts,
                                                 &topology->scanner->error)) {
      MONGOC_ERROR ("%s", topology->scanner->error.message);
      /* Special case when DNS returns zero records successfully or no valid
       * hosts exist.
       * Leave the toplogy alone and perform another scan at the next interval
       * rather than removing all records and having nothing to connect to.
       * For no verified hosts drivers "MUST temporarily set
       * srv_polling_rescan_interval_ms
       * to heartbeatFrequencyMS until at least one verified SRV record is
       * obtained."
       */
      topology->srv_polling_rescan_interval_ms =
         topology->description.heartbeat_msec;
      GOTO (done);
   }

done:
   bson_free (prefixed_service);
   _mongoc_host_list_destroy_all (rr_data.hosts);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scan_once --
 *
 *      Runs a single complete scan.
 *
 *      NOTE: this method expects @topology's mutex to be locked on entry.
 *
 *      NOTE: this method unlocks and re-locks @topology's mutex.
 *
 *      Only runs for single threaded monitoring. (obey_cooldown is always
 *      true).
 *
 *--------------------------------------------------------------------------
 */
static void
mongoc_topology_scan_once (mongoc_topology_t *topology, bool obey_cooldown)
{
   MONGOC_DEBUG_ASSERT (COMMON_PREFIX (mutex_is_locked) (&topology->mutex));

   if (mongoc_topology_should_rescan_srv (topology)) {
      /* Prior to scanning hosts, update the list of SRV hosts, if applicable.
       */
      mongoc_topology_rescan_srv (topology);
   }

   /* since the last scan, members may be added or removed from the topology
    * description based on hello responses in connection handshakes, see
    * _mongoc_topology_update_from_handshake. retire scanner nodes for removed
    * members and create scanner nodes for new ones. */
   mongoc_topology_reconcile (topology);
   mongoc_topology_scanner_start (topology->scanner, obey_cooldown);

   /* scanning locks and unlocks the mutex itself until the scan is done */
   bson_mutex_unlock (&topology->mutex);
   mongoc_topology_scanner_work (topology->scanner);

   bson_mutex_lock (&topology->mutex);

   _mongoc_topology_scanner_finish (topology->scanner);

   topology->last_scan = bson_get_monotonic_time ();
   topology->stale = false;
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
void
_mongoc_topology_do_blocking_scan (mongoc_topology_t *topology,
                                   bson_error_t *error)
{
   _mongoc_handshake_freeze ();

   bson_mutex_lock (&topology->mutex);
   mongoc_topology_scan_once (topology, true /* obey cooldown */);
   bson_mutex_unlock (&topology->mutex);
   mongoc_topology_scanner_get_error (topology->scanner, error);
}


bool
mongoc_topology_compatible (const mongoc_topology_description_t *td,
                            const mongoc_read_prefs_t *read_prefs,
                            bson_error_t *error)
{
   int64_t max_staleness_seconds;
   int32_t max_wire_version;

   if (td->compatibility_error.code) {
      if (error) {
         memcpy (error, &td->compatibility_error, sizeof (bson_error_t));
      }
      return false;
   }

   if (!read_prefs) {
      /* NULL means read preference Primary */
      return true;
   }

   max_staleness_seconds =
      mongoc_read_prefs_get_max_staleness_seconds (read_prefs);

   if (max_staleness_seconds != MONGOC_NO_MAX_STALENESS) {
      max_wire_version =
         mongoc_topology_description_lowest_max_wire_version (td);

      if (max_wire_version < WIRE_VERSION_MAX_STALENESS) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         "Not all servers support maxStalenessSeconds");
         return false;
      }

      /* shouldn't happen if we've properly enforced wire version */
      if (!mongoc_topology_description_all_sds_have_write_date (td)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         "Not all servers have lastWriteDate");
         return false;
      }

      if (!_mongoc_topology_description_validate_max_staleness (
             td, max_staleness_seconds, error)) {
         return false;
      }
   }

   return true;
}


static void
_mongoc_server_selection_error (const char *msg,
                                const bson_error_t *scanner_error,
                                bson_error_t *error)
{
   if (scanner_error && scanner_error->code) {
      bson_set_error (error,
                      MONGOC_ERROR_SERVER_SELECTION,
                      MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                      "%s: %s",
                      msg,
                      scanner_error->message);
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_SERVER_SELECTION,
                      MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                      "%s",
                      msg);
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_select --
 *
 *       Selects a server description for an operation based on @optype
 *       and @read_prefs.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 *       NOTE: this method locks and unlocks @topology's mutex.
 *
 * Parameters:
 *       @topology: The topology.
 *       @optype: Whether we are selecting for a read or write operation.
 *       @read_prefs: Required, the read preferences for the command.
 *       @error: Required, out pointer for error info.
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
mongoc_topology_select (mongoc_topology_t *topology,
                        mongoc_ss_optype_t optype,
                        const mongoc_read_prefs_t *read_prefs,
                        bson_error_t *error)
{
   uint32_t server_id =
      mongoc_topology_select_server_id (topology, optype, read_prefs, error);

   if (server_id) {
      /* new copy of the server description */
      return mongoc_topology_server_by_id (topology, server_id, error);
   } else {
      return NULL;
   }
}

/* Bypasses normal server selection behavior for a load balanced topology.
 * Returns the id of the one load balancer server. Returns 0 on failure.
 * Successful post-condition: On a single threaded client, a connection will
 * have been established. */
static uint32_t
_mongoc_topology_select_server_id_loadbalanced (mongoc_topology_t *topology,
                                                bson_error_t *error)
{
   mongoc_server_description_t *selected_server;
   int32_t selected_server_id;
   mongoc_topology_scanner_node_t *node;
   bson_error_t scanner_error = {0};

   bson_mutex_lock (&topology->mutex);

   BSON_ASSERT (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED);

   /* Emit the opening SDAM events if they have not emitted already. */
   _mongoc_topology_description_monitor_opening (&topology->description);
   selected_server =
      mongoc_topology_description_select (&topology->description,
                                          MONGOC_SS_WRITE,
                                          NULL /* read prefs */,
                                          0 /* local threshold */);

   if (!selected_server) {
      _mongoc_server_selection_error (
         "No suitable server found in load balanced deployment", NULL, error);
      bson_mutex_unlock (&topology->mutex);
      return 0;
   }

   selected_server_id = selected_server->id;
   bson_mutex_unlock (&topology->mutex);

   if (!topology->single_threaded) {
      return selected_server_id;
   }

   /* If this is a single threaded topology, we must ensure that a connection is
    * available to this server. Wrapping drivers make the assumption that
    * successful server selection implies a connection is available. */
   node =
      mongoc_topology_scanner_get_node (topology->scanner, selected_server_id);
   if (!node) {
      _mongoc_server_selection_error (
         "Topology scanner in invalid state; cannot find load balancer",
         NULL,
         error);
      return 0;
   }

   if (!node->stream) {
      TRACE ("%s",
             "Server selection performing scan since no connection has "
             "been established");
      _mongoc_topology_do_blocking_scan (topology, &scanner_error);
   }

   if (!node->stream) {
      /* Use the same error domain / code that is returned in mongoc-cluster.c
       * when fetching a stream fails. */
      if (scanner_error.code) {
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                         "Could not establish stream for node %s: %s",
                         node->host.host_and_port,
                         scanner_error.message);
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                         "Could not establish stream for node %s",
                         node->host.host_and_port);
      }
      return 0;
   }

   return selected_server_id;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_select_server_id --
 *
 *       Alternative to mongoc_topology_select when you only need the id.
 *
 * Returns:
 *       A server id, or 0 on failure, in which case @error will be set.
 *
 *-------------------------------------------------------------------------
 */
uint32_t
mongoc_topology_select_server_id (mongoc_topology_t *topology,
                                  mongoc_ss_optype_t optype,
                                  const mongoc_read_prefs_t *read_prefs,
                                  bson_error_t *error)
{
   static const char *timeout_msg =
      "No suitable servers found: `serverSelectionTimeoutMS` expired";

   mongoc_topology_scanner_t *ts;
   int r;
   int64_t local_threshold_ms;
   mongoc_server_description_t *selected_server = NULL;
   bool try_once;
   int64_t sleep_usec;
   bool tried_once;
   bson_error_t scanner_error = {0};
   int64_t heartbeat_msec;
   uint32_t server_id;

   /* These names come from the Server Selection Spec pseudocode */
   int64_t loop_start;  /* when we entered this function */
   int64_t loop_end;    /* when we last completed a loop (single-threaded) */
   int64_t scan_ready;  /* the soonest we can do a blocking scan */
   int64_t next_update; /* the latest we must do a blocking scan */
   int64_t expire_at;   /* when server selection timeout expires */

   BSON_ASSERT (topology);
   ts = topology->scanner;

   bson_mutex_lock (&topology->mutex);
   /* It isn't strictly necessary to lock here, because if the topology
    * is invalid, it will never become valid. Lock anyway for consistency. */
   if (!mongoc_topology_scanner_valid (ts)) {
      if (error) {
         mongoc_topology_scanner_get_error (ts, error);
         error->domain = MONGOC_ERROR_SERVER_SELECTION;
         error->code = MONGOC_ERROR_SERVER_SELECTION_FAILURE;
      }
      bson_mutex_unlock (&topology->mutex);
      return 0;
   }

   if (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED) {
      bson_mutex_unlock (&topology->mutex);
      return _mongoc_topology_select_server_id_loadbalanced (topology, error);
   }

   bson_mutex_unlock (&topology->mutex);

   heartbeat_msec = topology->description.heartbeat_msec;
   local_threshold_ms = topology->local_threshold_msec;
   try_once = topology->server_selection_try_once;
   loop_start = loop_end = bson_get_monotonic_time ();
   expire_at =
      loop_start + ((int64_t) topology->server_selection_timeout_msec * 1000);

   if (topology->single_threaded) {
      _mongoc_topology_description_monitor_opening (&topology->description);

      tried_once = false;
      next_update = topology->last_scan + heartbeat_msec * 1000;
      if (next_update < loop_start) {
         /* we must scan now */
         topology->stale = true;
      }

      /* until we find a server or time out */
      for (;;) {
         if (topology->stale) {
            /* how soon are we allowed to scan? */
            scan_ready = topology->last_scan +
                         topology->min_heartbeat_frequency_msec * 1000;

            if (scan_ready > expire_at && !try_once) {
               /* selection timeout will expire before min heartbeat passes */
               _mongoc_server_selection_error (
                  "No suitable servers found: "
                  "`serverselectiontimeoutms` timed out",
                  &scanner_error,
                  error);

               return 0;
            }

            sleep_usec = scan_ready - loop_end;
            if (sleep_usec > 0) {
               if (try_once &&
                   mongoc_topology_scanner_in_cooldown (ts, scan_ready)) {
                  _mongoc_server_selection_error (
                     "No servers yet eligible for rescan",
                     &scanner_error,
                     error);

                  return 0;
               }

               _mongoc_usleep (sleep_usec);
            }

            /* takes up to connectTimeoutMS. sets "last_scan", clears "stale" */
            _mongoc_topology_do_blocking_scan (topology, &scanner_error);
            loop_end = topology->last_scan;
            tried_once = true;
         }

         if (!mongoc_topology_compatible (
                &topology->description, read_prefs, error)) {
            return 0;
         }

         selected_server = mongoc_topology_description_select (
            &topology->description, optype, read_prefs, local_threshold_ms);

         if (selected_server) {
            return selected_server->id;
         }

         topology->stale = true;

         if (try_once) {
            if (tried_once) {
               _mongoc_server_selection_error (
                  "No suitable servers found (`serverSelectionTryOnce` set)",
                  &scanner_error,
                  error);

               return 0;
            }
         } else {
            loop_end = bson_get_monotonic_time ();

            if (loop_end > expire_at) {
               /* no time left in server_selection_timeout_msec */
               _mongoc_server_selection_error (
                  timeout_msg, &scanner_error, error);

               return 0;
            }
         }
      }
   }

   /* With background thread */
   /* we break out when we've found a server or timed out */
   for (;;) {
      bson_mutex_lock (&topology->mutex);

      if (!mongoc_topology_compatible (
             &topology->description, read_prefs, error)) {
         bson_mutex_unlock (&topology->mutex);
         return 0;
      }

      selected_server = mongoc_topology_description_select (
         &topology->description, optype, read_prefs, local_threshold_ms);

      if (!selected_server) {
         TRACE (
            "server selection requesting an immediate scan, want %s",
            _mongoc_read_mode_as_str (mongoc_read_prefs_get_mode (read_prefs)));
         _mongoc_topology_request_scan (topology);

         TRACE ("server selection about to wait for %" PRId64 "ms",
                (expire_at - loop_start) / 1000);
         r = mongoc_cond_timedwait (&topology->cond_client,
                                    &topology->mutex,
                                    (expire_at - loop_start) / 1000);
         TRACE ("%s", "server selection awake");
         _topology_collect_errors (topology, &scanner_error);

         bson_mutex_unlock (&topology->mutex);

#ifdef _WIN32
         if (r == WSAETIMEDOUT) {
#else
         if (r == ETIMEDOUT) {
#endif
            /* handle timeouts */
            _mongoc_server_selection_error (timeout_msg, &scanner_error, error);

            return 0;
         } else if (r) {
            bson_set_error (error,
                            MONGOC_ERROR_SERVER_SELECTION,
                            MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                            "Unknown error '%d' received while waiting on "
                            "thread condition",
                            r);
            return 0;
         }

         loop_start = bson_get_monotonic_time ();

         if (loop_start > expire_at) {
            _mongoc_server_selection_error (timeout_msg, &scanner_error, error);

            return 0;
         }
      } else {
         server_id = selected_server->id;
         bson_mutex_unlock (&topology->mutex);
         return server_id;
      }
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_server_by_id --
 *
 *      Get the server description for @id, if that server is present
 *      in @description. Otherwise, return NULL and fill out the optional
 *      @error.
 *
 *      NOTE: this method returns a copy of the original server
 *      description. Callers must own and clean up this copy.
 *
 *      NOTE: this method locks and unlocks @topology's mutex.
 *
 * Returns:
 *      A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *      Fills out optional @error if server not found.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
mongoc_topology_server_by_id (mongoc_topology_t *topology,
                              uint32_t id,
                              bson_error_t *error)
{
   mongoc_server_description_t *sd;

   bson_mutex_lock (&topology->mutex);

   sd = mongoc_server_description_new_copy (
      mongoc_topology_description_server_by_id (
         &topology->description, id, error));

   bson_mutex_unlock (&topology->mutex);

   return sd;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_host_by_id --
 *
 *      Copy the mongoc_host_list_t for @id, if that server is present
 *      in @description. Otherwise, return NULL and fill out the optional
 *      @error.
 *
 *      NOTE: this method returns a copy of the original mongoc_host_list_t.
 *      Callers must own and clean up this copy.
 *
 *      NOTE: this method locks and unlocks @topology's mutex.
 *
 * Returns:
 *      A mongoc_host_list_t, or NULL.
 *
 * Side effects:
 *      Fills out optional @error if server not found.
 *
 *-------------------------------------------------------------------------
 */

mongoc_host_list_t *
_mongoc_topology_host_by_id (mongoc_topology_t *topology,
                             uint32_t id,
                             bson_error_t *error)
{
   mongoc_server_description_t *sd;
   mongoc_host_list_t *host = NULL;

   bson_mutex_lock (&topology->mutex);

   /* not a copy - direct pointer into topology description data */
   sd = mongoc_topology_description_server_by_id (
      &topology->description, id, error);

   if (sd) {
      host = bson_malloc0 (sizeof (mongoc_host_list_t));
      memcpy (host, &sd->host, sizeof (mongoc_host_list_t));
   }

   bson_mutex_unlock (&topology->mutex);

   return host;
}

/*

 * Caller must have topology->mutex locked.
 *
 */

void
_mongoc_topology_request_scan (mongoc_topology_t *topology)
{
   _mongoc_topology_background_monitoring_request_scan (topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_invalidate_server --
 *
 *      Invalidate the given server after receiving a network error in
 *      another part of the client.
 *
 *      NOTE: this method uses @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_topology_invalidate_server (mongoc_topology_t *topology,
                                   uint32_t id,
                                   const bson_error_t *error)
{
   BSON_ASSERT (error);

   bson_mutex_lock (&topology->mutex);
   mongoc_topology_description_invalidate_server (
      &topology->description, id, error);
   bson_mutex_unlock (&topology->mutex);
}

/*
 * Update the topology from the response to a handshake on a new application
 * connection.
 * Only applicable to a client pool (single-threaded clients reuse monitoring
 * connections).
 * Caller must not have the topology->mutex locked.
 * Locks topology->mutex.
 * Called only from app threads (not server monitor threads).
 * Returns false if the server was removed from the topology
 */
bool
_mongoc_topology_update_from_handshake (mongoc_topology_t *topology,
                                        const mongoc_server_description_t *sd)
{
   bool has_server;

   BSON_ASSERT (topology);
   BSON_ASSERT (sd);
   BSON_ASSERT (!topology->single_threaded);

   bson_mutex_lock (&topology->mutex);

   if (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED) {
      /* In load balanced mode, scanning is only for connection establishment.
       * It must not modify the topology description. */
      bson_mutex_unlock (&topology->mutex);
      return true;
   }

   /* return false if server was removed from topology */
   has_server = _mongoc_topology_update_no_lock (sd->id,
                                                 &sd->last_hello_response,
                                                 sd->round_trip_time_msec,
                                                 topology,
                                                 NULL);

   /* if pooled, wake threads waiting in mongoc_topology_server_by_id */
   mongoc_cond_broadcast (&topology->cond_client);
   /* Update background monitoring. */
   _mongoc_topology_background_monitoring_reconcile (topology);
   bson_mutex_unlock (&topology->mutex);

   return has_server;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_update_last_used --
 *
 *       Internal function. In single-threaded mode only, track when the socket
 *       to a particular server was last used. This is required for
 *       mongoc_cluster_check_interval to know when a socket has been idle.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_topology_update_last_used (mongoc_topology_t *topology,
                                   uint32_t server_id)
{
   mongoc_topology_scanner_node_t *node;

   if (!topology->single_threaded) {
      return;
   }

   node = mongoc_topology_scanner_get_node (topology->scanner, server_id);
   if (node) {
      node->last_used = bson_get_monotonic_time ();
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_get_type --
 *
 *      Return the topology's description's type.
 *
 *      NOTE: this method uses @topology's mutex.
 *
 * Returns:
 *      The topology description type.
 *
 *--------------------------------------------------------------------------
 */
mongoc_topology_description_type_t
_mongoc_topology_get_type (mongoc_topology_t *topology)
{
   mongoc_topology_description_type_t td_type;

   bson_mutex_lock (&topology->mutex);

   td_type = topology->description.type;

   bson_mutex_unlock (&topology->mutex);

   return td_type;
}

bool
_mongoc_topology_set_appname (mongoc_topology_t *topology, const char *appname)
{
   bool ret = false;
   bson_mutex_lock (&topology->mutex);

   if (topology->scanner_state == MONGOC_TOPOLOGY_SCANNER_OFF) {
      ret = _mongoc_topology_scanner_set_appname (topology->scanner, appname);
   } else {
      MONGOC_ERROR ("Cannot set appname after handshake initiated");
   }
   bson_mutex_unlock (&topology->mutex);
   return ret;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_update_cluster_time --
 *
 *       Internal function. If the server reply has a later $clusterTime than
 *       any seen before, update the topology's clusterTime. See the Driver
 *       Sessions Spec.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_topology_update_cluster_time (mongoc_topology_t *topology,
                                      const bson_t *reply)
{
   bson_mutex_lock (&topology->mutex);
   mongoc_topology_description_update_cluster_time (&topology->description,
                                                    reply);
   _mongoc_topology_scanner_set_cluster_time (
      topology->scanner, &topology->description.cluster_time);
   bson_mutex_unlock (&topology->mutex);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_pop_server_session --
 *
 *       Internal function. Get a server session from the pool or create
 *       one. On error, return NULL and fill out @error.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_session_t *
_mongoc_topology_pop_server_session (mongoc_topology_t *topology,
                                     bson_error_t *error)
{
   int64_t timeout;
   mongoc_server_session_t *ss = NULL;
   mongoc_topology_description_t *td;
   bool loadbalanced;

   ENTRY;

   bson_mutex_lock (&topology->mutex);

   td = &topology->description;
   timeout = td->session_timeout_minutes;
   loadbalanced = td->type == MONGOC_TOPOLOGY_LOAD_BALANCED;

   /* When the topology type is LoadBalanced, sessions are always supported. */
   if (!loadbalanced && timeout == MONGOC_NO_SESSIONS) {
      /* if needed, connect and check for session timeout again */
      if (!mongoc_topology_description_has_data_node (td)) {
         bson_mutex_unlock (&topology->mutex);
         if (!mongoc_topology_select_server_id (
                topology, MONGOC_SS_READ, NULL, error)) {
            RETURN (NULL);
         }

         bson_mutex_lock (&topology->mutex);
         timeout = td->session_timeout_minutes;
      }

      if (timeout == MONGOC_NO_SESSIONS) {
         bson_mutex_unlock (&topology->mutex);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_SESSION_FAILURE,
                         "Server does not support sessions");
         RETURN (NULL);
      }
   }
   bson_mutex_unlock (&topology->mutex);

   ss = mongoc_server_session_pool_get (topology->session_pool, error);

   RETURN (ss);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_push_server_session --
 *
 *       Internal function. Return a server session to the pool.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_topology_push_server_session (mongoc_topology_t *topology,
                                      mongoc_server_session_t *server_session)
{
   ENTRY;

   /**
    * ! note:
    * At time of writing, this diverges from the spec:
    * https://github.com/mongodb/specifications/blob/df6be82f865e9b72444488fd62ae1eb5fca18569/source/sessions/driver-sessions.rst#algorithm-to-return-a-serversession-instance-to-the-server-session-pool
    *
    * The spec notes that before returning a session, we should first inspect
    * the back of the pool for expired items and delete them. In this case, we
    * simply return the item to the top of the pool and leave the remainder
    * unchanged.
    *
    * The next pop operation that encounters an expired session will clear the
    * entire session pool, thus preventing unbounded growth of the pool.
    */
   mongoc_server_session_pool_return (server_session);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_end_sessions_cmd --
 *
 *       Internal function. End up to 10,000 server sessions. @cmd is an
 *       uninitialized document. Sessions are destroyed as their ids are
 *       appended to @cmd.
 *
 *       Driver Sessions Spec: "If the number of sessions is very large the
 *       endSessions command SHOULD be run multiple times to end 10,000
 *       sessions at a time (in order to avoid creating excessively large
 *       commands)."
 *
 * Returns:
 *      true if any session ids were appended to @cmd.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_topology_end_sessions_cmd (mongoc_topology_t *topology, bson_t *cmd)
{
   bson_t ar;
   /* Only end up to 10'000 sessions */
   const int ENDED_SESSION_PRUNING_LIMIT = 10000;
   int i = 0;
   mongoc_server_session_t *ss =
      mongoc_server_session_pool_get_existing (topology->session_pool);

   bson_init (cmd);
   BSON_APPEND_ARRAY_BEGIN (cmd, "endSessions", &ar);

   for (; i < ENDED_SESSION_PRUNING_LIMIT && ss != NULL;
        ++i,
        ss = mongoc_server_session_pool_get_existing (topology->session_pool)) {
      char buf[16];
      const char *key;
      bson_uint32_to_string (i, &key, buf, sizeof buf);
      BSON_APPEND_DOCUMENT (&ar, key, &ss->lsid);
      mongoc_server_session_pool_drop (ss);
   }

   if (ss) {
      /* We deleted at least 10'000 sessions, so we will need to return the
       * final session that we didn't drop */
      mongoc_server_session_pool_return (ss);
   }

   bson_append_array_end (cmd, &ar);

   return i > 0;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_get_handshake_cmd --
 *
 *       Locks topology->mutex and retrieves (possibly constructing) the
 *       handshake on the topology scanner.
 *
 * Returns:
 *      A bson_t representing a hello command.
 *
 *--------------------------------------------------------------------------
 */
const bson_t *
_mongoc_topology_get_handshake_cmd (mongoc_topology_t *topology)
{
   const bson_t *cmd;
   bson_mutex_lock (&topology->mutex);
   cmd = _mongoc_topology_scanner_get_handshake_cmd (topology->scanner);
   bson_mutex_unlock (&topology->mutex);
   return cmd;
}

void
_mongoc_topology_bypass_cooldown (mongoc_topology_t *topology)
{
   BSON_ASSERT (topology->single_threaded);
   topology->scanner->bypass_cooldown = true;
}

static void
_find_topology_version (const bson_t *reply, bson_t *topology_version)
{
   bson_iter_t iter;
   const uint8_t *bytes;
   uint32_t len;

   if (!bson_iter_init_find (&iter, reply, "topologyVersion") ||
       !BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      bson_init (topology_version);
      return;
   }
   bson_iter_document (&iter, &len, &bytes);
   bson_init_static (topology_version, bytes, len);
}


/* "Clears" the connection pool by incrementing the generation.
 *
 * Pooled clients with open connections will discover the invalidation
 * the next time they fetch a stream to the server.
 *
 * Caller must lock topology->mutex. */
void
_mongoc_topology_clear_connection_pool (mongoc_topology_t *topology,
                                        uint32_t server_id,
                                        const bson_oid_t *service_id)
{
   mongoc_server_description_t *sd;
   bson_error_t error;

   BSON_ASSERT (service_id);

   sd = mongoc_topology_description_server_by_id (
      &topology->description, server_id, &error);
   if (!sd) {
      /* Server removed, ignore and ignore error. */
      return;
   }

   TRACE ("clearing pool for server: %s", sd->host.host_and_port);

   mongoc_generation_map_increment (sd->generation_map, service_id);
}


/* Handle an error from an app connection.
 *
 * This can be a network error or "not primary" / "node is recovering" error.
 * Caller must lock topology->mutex.
 * service_id is only applicable if connected to a load balanced deployment.
 * Pass kZeroServiceID as service_id for connections that have no
 * associated service ID.
 * Returns true if pool was cleared.
 */
bool
_mongoc_topology_handle_app_error (mongoc_topology_t *topology,
                                   uint32_t server_id,
                                   bool handshake_complete,
                                   _mongoc_sdam_app_error_type_t type,
                                   const bson_t *reply,
                                   const bson_error_t *why,
                                   uint32_t max_wire_version,
                                   uint32_t generation,
                                   const bson_oid_t *service_id)
{
   bson_error_t server_selection_error;
   mongoc_server_description_t *sd;
   bool pool_cleared;

   pool_cleared = false;
   sd = mongoc_topology_description_server_by_id (
      &topology->description, server_id, &server_selection_error);
   if (!sd) {
      /* The server was already removed from the topology. Ignore error. */
      return false;
   }

   /* When establishing a new connection in load balanced mode, drivers MUST NOT
    * perform SDAM error handling for any errors that occur before the MongoDB
    * Handshake. */
   if (topology->description.type == MONGOC_TOPOLOGY_LOAD_BALANCED &&
       !handshake_complete) {
      return false;
   }

   if (generation < _mongoc_topology_get_connection_pool_generation (
                       topology, server_id, service_id)) {
      /* This is a stale connection. Ignore. */
      return false;
   }

   if (type == MONGOC_SDAM_APP_ERROR_NETWORK) {
      /* Mark server as unknown. */
      mongoc_topology_description_invalidate_server (
         &topology->description, server_id, why);
      _mongoc_topology_clear_connection_pool (topology, server_id, service_id);
      pool_cleared = true;
      if (!topology->single_threaded) {
         _mongoc_topology_background_monitoring_cancel_check (topology,
                                                              server_id);
      }
   } else if (type == MONGOC_SDAM_APP_ERROR_TIMEOUT) {
      if (handshake_complete) {
         /* Timeout errors after handshake are ok, do nothing. */
         return false;
      }
      /* Mark server as unknown. */
      mongoc_topology_description_invalidate_server (
         &topology->description, server_id, why);
      _mongoc_topology_clear_connection_pool (topology, server_id, service_id);
      pool_cleared = true;
      if (!topology->single_threaded) {
         _mongoc_topology_background_monitoring_cancel_check (topology,
                                                              server_id);
      }
   } else if (type == MONGOC_SDAM_APP_ERROR_COMMAND) {
      bson_error_t cmd_error;
      bson_t incoming_topology_version;

      if (_mongoc_cmd_check_ok_no_wce (
             reply, MONGOC_ERROR_API_VERSION_2, &cmd_error)) {
         /* No error. */
         return false;
      }

      if (!_mongoc_error_is_state_change (&cmd_error)) {
         /* Not a "not primary" or "node is recovering" error. */
         return false;
      }

      /* Check if the error is "stale", i.e. the topologyVersion refers to an
       * older
       * version of the server than we have stored in the topology description.
       */
      _find_topology_version (reply, &incoming_topology_version);
      if (mongoc_server_description_topology_version_cmp (
             &sd->topology_version, &incoming_topology_version) >= 0) {
         /* The server description is greater or equal, ignore the error. */
         bson_destroy (&incoming_topology_version);
         return false;
      }
      /* Overwrite the topology version. */
      mongoc_server_description_set_topology_version (
         sd, &incoming_topology_version);
      bson_destroy (&incoming_topology_version);

      /* SDAM: When handling a "not primary" or "node is recovering" error, the
       * client MUST clear the server's connection pool if and only if the error
       * is "node is shutting down" or the error originated from server version
       * < 4.2.
       */
      if (max_wire_version <= WIRE_VERSION_4_0 ||
          _mongoc_error_is_shutdown (&cmd_error)) {
         _mongoc_topology_clear_connection_pool (
            topology, server_id, service_id);
         pool_cleared = true;
      }

      /* SDAM: When the client sees a "not primary" or "node is recovering"
       * error and the error's topologyVersion is strictly greater than the
       * current ServerDescription's topologyVersion it MUST replace the
       * server's description with a ServerDescription of type Unknown. */
      mongoc_topology_description_invalidate_server (
         &topology->description, server_id, &cmd_error);

      if (topology->single_threaded) {
         /* SDAM: For single-threaded clients, in the case of a "not primary" or
          * "node is shutting down" error, the client MUST mark the topology as
          * "stale"
          */
         if (_mongoc_error_is_not_primary (&cmd_error)) {
            topology->stale = true;
         }
      } else {
         /* SDAM Spec: "Multi-threaded and asynchronous clients MUST request an
          * immediate check of the server."
          * Instead of requesting a check of the one server, request a scan
          * to all servers (to find the new primary).
          */
         _mongoc_topology_request_scan (topology);
      }
   }
   return pool_cleared;
}

/* Called from application threads
 * Caller must hold topology lock.
 * Locks topology description mutex to copy out server description errors.
 * For single-threaded monitoring, the topology scanner may include errors for
 * servers that were removed from the topology.
 */
static void
_topology_collect_errors (mongoc_topology_t *topology, bson_error_t *error_out)
{
   mongoc_topology_description_t *topology_description;
   mongoc_server_description_t *server_description;
   bson_string_t *error_message;
   int i;

   topology_description = &topology->description;
   memset (error_out, 0, sizeof (bson_error_t));
   error_message = bson_string_new ("");

   for (i = 0; i < topology_description->servers->items_len; i++) {
      bson_error_t *error;

      server_description = topology_description->servers->items[i].item;
      error = &server_description->error;
      if (error->code) {
         if (error_message->len > 0) {
            bson_string_append_c (error_message, ' ');
         }
         bson_string_append_printf (
            error_message, "[%s]", server_description->error.message);
         /* The last error's code and domain wins. */
         error_out->code = error->code;
         error_out->domain = error->domain;
      }
   }

   bson_strncpy ((char *) &error_out->message,
                 error_message->str,
                 sizeof (error_out->message));
   bson_string_free (error_message, true);
}

void
_mongoc_topology_set_rr_resolver (mongoc_topology_t *topology,
                                  _mongoc_rr_resolver_fn rr_resolver)
{
   MONGOC_DEBUG_ASSERT (COMMON_PREFIX (mutex_is_locked) (&topology->mutex));
   topology->rr_resolver = rr_resolver;
}

void
_mongoc_topology_set_srv_polling_rescan_interval_ms (
   mongoc_topology_t *topology, int64_t val)
{
   MONGOC_DEBUG_ASSERT (COMMON_PREFIX (mutex_is_locked) (&topology->mutex));
   topology->srv_polling_rescan_interval_ms = val;
}

uint32_t
_mongoc_topology_get_connection_pool_generation (mongoc_topology_t *topology,
                                                 uint32_t server_id,
                                                 const bson_oid_t *service_id)
{
   mongoc_server_description_t *sd;
   bson_error_t error;

   BSON_ASSERT (service_id);

   sd = mongoc_topology_description_server_by_id (
      &topology->description, server_id, &error);
   if (!sd) {
      /* Server removed, ignore and ignore error. */
      return 0;
   }

   return mongoc_generation_map_get (sd->generation_map, service_id);
}
