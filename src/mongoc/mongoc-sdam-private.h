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
#include "mongoc-server-description.h"
#include "mongoc-topology-description.h"
#include "mongoc-uri.h"

#define MONGOC_SDAM_MIN_HEARTBEAT_FREQUENCY_MS 60000
#define MONGOC_SDAM_SOCKET_CHECK_INTERVAL_MS 5000 // must be configurable
#define MONGOC_SDAM_HEARTBEAT_FREQUENCY_MS 60000 // change: must be configurable

typedef struct _mongoc_sdam_t
{
   mongoc_topology_description_t topology;
   mongoc_uri_t                  *uri;
   transition_table_t             transitions;
   // TODO SCAN jason's scanner thing
} mongoc_sdam_t;

mongoc_sdam_t               *_mongoc_sdam_new               (mongoc_uri_t  *uri);
void                         _mongoc_sdam_destroy           (mongoc_sdam_t *sdam);
void                         _mongoc_sdam_force_scan        (mongoc_sdam_t *sdam);
void                         _mongoc_sdam_ismaster_callback (mongoc_sdam_t *sdam,
                                                             const bson_t  *ismaster);
#endif
