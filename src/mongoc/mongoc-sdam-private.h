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

#ifndef MONGOC_SDAM_PRIVATE_H
#define MONGOC_SDAM_PRIVATE_H

#include "mongoc-read-prefs-private.h"
#include "mongoc-sdam-scanner-private.h"
#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"
#include "mongoc-thread-private.h"
#include "mongoc-uri.h"

#define MONGOC_SDAM_MIN_HEARTBEAT_FREQUENCY_MS 60000
#define MONGOC_SDAM_SOCKET_CHECK_INTERVAL_MS 5000 // must be configurable
#define MONGOC_SDAM_HEARTBEAT_FREQUENCY_MS 60000 // change: must be configurable

typedef enum {
   MONGOC_SDAM_BG_OFF,
   MONGOC_SDAM_BG_RUNNING,
   MONGOC_SDAM_BG_SHUTTING_DOWN,
} mongoc_sdam_bg_state_t;

typedef struct _mongoc_sdam_t
{
   mongoc_topology_description_t topology;
   const mongoc_uri_t           *uri;
   int                           users;
   mongoc_sdam_scanner_t        *scanner;
   int64_t                       last_scan;
   bool                          scan_requested;
   int64_t                       timeout_msec;
   mongoc_mutex_t                mutex;
   mongoc_cond_t                 cond_client;
   mongoc_cond_t                 cond_server;
   mongoc_thread_t               thread;
   mongoc_sdam_bg_state_t        bg_thread_state;
   bool                          scanning;
   bool                          got_ismaster;
   int64_t                       heartbeat_msec;
   bool                          shutdown_requested;
} mongoc_sdam_t;

mongoc_sdam_t *
_mongoc_sdam_new (const mongoc_uri_t *uri);

void
_mongoc_sdam_grab (mongoc_sdam_t *sdam);

void
_mongoc_sdam_release (mongoc_sdam_t *sdam);

void
_mongoc_sdam_destroy (mongoc_sdam_t *sdam);

mongoc_server_description_t *
_mongoc_sdam_select (mongoc_sdam_t             *sdam,
                     mongoc_ss_optype_t         optype,
                     const mongoc_read_prefs_t *read_prefs,
                     int64_t                    timeout_msec,
                     bson_error_t              *error);

mongoc_server_description_t *
_mongoc_sdam_server_by_id (mongoc_sdam_t *sdam,
                           uint32_t       id);

void
_mongoc_sdam_start_scan (mongoc_sdam_t *sdam);

bool
_mongoc_sdam_scan (mongoc_sdam_t *sdam,
                   int64_t        work_msec);

mongoc_sdam_bg_state_t
_mongoc_sdam_background_thread_state (mongoc_sdam_t *sdam);

#endif
