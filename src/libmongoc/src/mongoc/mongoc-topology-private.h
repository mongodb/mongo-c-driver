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

#include "mongoc-prelude.h"

#ifndef MONGOC_TOPOLOGY_PRIVATE_H
#define MONGOC_TOPOLOGY_PRIVATE_H

#include "mongoc-config.h"
#include "mongoc-topology-scanner-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-thread-private.h"
#include "mongoc-uri.h"
#include "mongoc-client-session-private.h"
#include "mongoc-crypt-private.h"
#include "mongoc-ts-pool-private.h"

#define MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS 500
#define MONGOC_TOPOLOGY_SOCKET_CHECK_INTERVAL_MS 5000
#define MONGOC_TOPOLOGY_COOLDOWN_MS 5000
#define MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS 15
#define MONGOC_TOPOLOGY_SERVER_SELECTION_TIMEOUT_MS 30000
#define MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_MULTI_THREADED 10000
#define MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED 60000
#define MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS 60000

typedef enum {
   MONGOC_TOPOLOGY_SCANNER_OFF,
   MONGOC_TOPOLOGY_SCANNER_BG_RUNNING,
   MONGOC_TOPOLOGY_SCANNER_SHUTTING_DOWN
} mongoc_topology_scanner_state_t;

struct _mongoc_background_monitor_t;
struct _mongoc_client_pool_t;

typedef enum { MONGOC_RR_SRV, MONGOC_RR_TXT } mongoc_rr_type_t;

typedef struct _mongoc_rr_data_t {
   /* Number of records returned by DNS. */
   uint32_t count;

   /* Set to lowest TTL found when polling SRV records. */
   uint32_t min_ttl;

   /* Set to the resulting host list when polling SRV records */
   mongoc_host_list_t *hosts;

   /* Set to the TXT record when polling for TXT */
   char *txt_record_opts;
} mongoc_rr_data_t;

struct _mongoc_topology_t;

MONGOC_DECL_SPECIAL_TS_POOL (
   mongoc_server_session_t,
   mongoc_server_session_pool,
   struct _mongoc_topology_t,
   /* ctor/dtor/prune are defined in the new_with_params call */
   NULL,
   NULL,
   NULL)

typedef bool (*_mongoc_rr_resolver_fn) (const char *service,
                                        mongoc_rr_type_t rr_type,
                                        mongoc_rr_data_t *rr_data,
                                        size_t initial_buffer_size,
                                        bson_error_t *error);

typedef struct _mongoc_topology_t {
   mongoc_topology_description_t description;
   /* topology->uri is initialized as a copy of the client/pool's URI.
    * For a "mongodb+srv://" URI, topology->uri is then updated in
    * mongoc_topology_new() after initial seedlist discovery.
    * Afterwards, it remains read-only and may be read outside of the topology
    * mutex.
    */
   mongoc_uri_t *uri;
   mongoc_topology_scanner_t *scanner;
   bool server_selection_try_once;

   int64_t last_scan;
   int64_t local_threshold_msec;
   int64_t connect_timeout_msec;
   int64_t server_selection_timeout_msec;
   /* defaults to 500ms, configurable by tests */
   int64_t min_heartbeat_frequency_msec;

   /* Minimum of SRV record TTLs, but no lower than 60 seconds.
    * May be zero for non-SRV/non-MongoS topology. */
   int64_t srv_polling_rescan_interval_ms;
   int64_t srv_polling_last_scan_ms;
   /* For multi-threaded, srv polling occurs in a separate thread. */
   bson_thread_t srv_polling_thread;
   mongoc_cond_t srv_polling_cond;

   bson_mutex_t mutex;
   mongoc_cond_t cond_client;
   mongoc_topology_scanner_state_t scanner_state;

   bool single_threaded;
   bool stale;

   mongoc_server_session_pool session_pool;

   /* Is client side encryption enabled? */
   bool cse_enabled;
   bool is_srv_polling;

#ifdef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION
   _mongoc_crypt_t *crypt;
   struct _mongoc_client_t *mongocryptd_client;           /* single threaded */
   struct _mongoc_client_t *keyvault_client;              /* single threaded */
   struct _mongoc_client_pool_t *mongocryptd_client_pool; /* multi threaded */
   struct _mongoc_client_pool_t *keyvault_client_pool;    /* multi threaded */
   char *keyvault_db;
   char *keyvault_coll;
   bool bypass_auto_encryption;
   bool mongocryptd_bypass_spawn;
   char *mongocryptd_spawn_path;
   bson_t *mongocryptd_spawn_args;
#endif

   /* For background monitoring. */
   mongoc_set_t *server_monitors;
   mongoc_set_t *rtt_monitors;
   bson_mutex_t apm_mutex;

   /* This is overridable for SRV polling tests to mock DNS records. */
   _mongoc_rr_resolver_fn rr_resolver;
} mongoc_topology_t;

mongoc_topology_t *
mongoc_topology_new (const mongoc_uri_t *uri, bool single_threaded);

void
mongoc_topology_set_apm_callbacks (mongoc_topology_t *topology,
                                   mongoc_apm_callbacks_t *callbacks,
                                   void *context);

void
mongoc_topology_destroy (mongoc_topology_t *topology);

void
mongoc_topology_reconcile (mongoc_topology_t *topology);

bool
mongoc_topology_compatible (const mongoc_topology_description_t *td,
                            const mongoc_read_prefs_t *read_prefs,
                            bson_error_t *error);

mongoc_server_description_t *
mongoc_topology_select (mongoc_topology_t *topology,
                        mongoc_ss_optype_t optype,
                        const mongoc_read_prefs_t *read_prefs,
                        bson_error_t *error);

uint32_t
mongoc_topology_select_server_id (mongoc_topology_t *topology,
                                  mongoc_ss_optype_t optype,
                                  const mongoc_read_prefs_t *read_prefs,
                                  bson_error_t *error);

mongoc_server_description_t *
mongoc_topology_server_by_id (mongoc_topology_t *topology,
                              uint32_t id,
                              bson_error_t *error);

mongoc_host_list_t *
_mongoc_topology_host_by_id (mongoc_topology_t *topology,
                             uint32_t id,
                             bson_error_t *error);

/* TODO: Try to remove this function when CDRIVER-3654 is complete.
 * It is only called when an application thread needs to mark a server Unknown.
 * But an application error is also tied to other behavior, and should also
 * consider the connection generation. This logic is captured in
 * _mongoc_topology_handle_app_error. This should not be called directly
 */
void
mongoc_topology_invalidate_server (mongoc_topology_t *topology,
                                   uint32_t id,
                                   const bson_error_t *error);

bool
_mongoc_topology_update_from_handshake (mongoc_topology_t *topology,
                                        const mongoc_server_description_t *sd);

void
_mongoc_topology_update_last_used (mongoc_topology_t *topology,
                                   uint32_t server_id);

int64_t
mongoc_topology_server_timestamp (mongoc_topology_t *topology, uint32_t id);

mongoc_topology_description_type_t
_mongoc_topology_get_type (mongoc_topology_t *topology);

bool
_mongoc_topology_set_appname (mongoc_topology_t *topology, const char *appname);

void
_mongoc_topology_update_cluster_time (mongoc_topology_t *topology,
                                      const bson_t *reply);

mongoc_server_session_t *
_mongoc_topology_pop_server_session (mongoc_topology_t *topology,
                                     bson_error_t *error);

void
_mongoc_topology_push_server_session (mongoc_topology_t *topology,
                                      mongoc_server_session_t *server_session);

bool
_mongoc_topology_end_sessions_cmd (mongoc_topology_t *topology, bson_t *cmd);

void
_mongoc_topology_do_blocking_scan (mongoc_topology_t *topology,
                                   bson_error_t *error);
const bson_t *
_mongoc_topology_get_handshake_cmd (mongoc_topology_t *topology);
void
_mongoc_topology_request_scan (mongoc_topology_t *topology);

void
_mongoc_topology_bypass_cooldown (mongoc_topology_t *topology);

typedef enum {
   MONGOC_SDAM_APP_ERROR_COMMAND,
   MONGOC_SDAM_APP_ERROR_NETWORK,
   MONGOC_SDAM_APP_ERROR_TIMEOUT
} _mongoc_sdam_app_error_type_t;

bool
_mongoc_topology_handle_app_error (mongoc_topology_t *topology,
                                   uint32_t server_id,
                                   bool handshake_complete,
                                   _mongoc_sdam_app_error_type_t type,
                                   const bson_t *reply,
                                   const bson_error_t *why,
                                   uint32_t max_wire_version,
                                   uint32_t generation,
                                   const bson_oid_t *service_id);

/* Invalidate open connections to a server.
 * This is not applicable to single-threaded clients, which only have one
 * or zero connections to any single server.
 * service_id is only applicable to load balanced deployments.
 * Pass kZeroServiceID as service_id to clear connections that have no
 * associated service ID. */
void
_mongoc_topology_clear_connection_pool (mongoc_topology_t *topology,
                                        uint32_t server_id,
                                        const bson_oid_t *service_id);

void
mongoc_topology_rescan_srv (mongoc_topology_t *topology);

bool
mongoc_topology_should_rescan_srv (mongoc_topology_t *topology);

/* _mongoc_topology_set_rr_resolver is called by tests to mock DNS responses for
 * SRV polling.
 * This is necessarily called after initial seedlist discovery completes in
 * mongoc_topology_new.
 * Callers should call this before monitoring starts.
 * Callers must lock topology->mutex.
 */
void
_mongoc_topology_set_rr_resolver (mongoc_topology_t *topology,
                                  _mongoc_rr_resolver_fn rr_resolver);

/* _mongoc_topology_set_srv_polling_rescan_interval_ms is called by tests to
 * shorten the rescan interval.
 * Callers should call this before monitoring starts.
 * Callers must lock topology->mutex.
 */
void
_mongoc_topology_set_srv_polling_rescan_interval_ms (
   mongoc_topology_t *topology, int64_t val);

/* Return the latest connection generation for the server_id and/or service_id.
 * Use this generation for newly established connections.
 * Pass kZeroServiceID connections do not have an associated service ID.
 * Callers must lock topology->mutex if topology is pooled. */
uint32_t
_mongoc_topology_get_connection_pool_generation (mongoc_topology_t *topology,
                                                 uint32_t server_id,
                                                 const bson_oid_t *service_id);

#endif
