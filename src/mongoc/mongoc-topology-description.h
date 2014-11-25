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

#ifndef MONGOC_TOPOLOGY_DESCRIPTION_H
#define MONGOC_TOPOLOGY_DESCRIPTION_H

#include "mongoc-read-prefs-private.h"
#include "mongoc-server-description.h"

#define MONGOC_TOPOLOGY_DESCRIPTION_TYPES 4 // TODO: what about Single??

#define MONGOC_SS_DEFAULT_TIMEOUT_MS 30000
#define MONGOC_SS_DEFAULT_LOCAL_THRESHOLD_MS 15

typedef enum
   {
      MONGOC_TOPOLOGY_SINGLE,
      MONGOC_TOPOLOGY_RS_NO_PRIMARY,
      MONGOC_TOPOLOGY_RS_WITH_PRIMARY,
      MONGOC_TOPOLOGY_SHARDED,
      MONGOC_TOPOLOGY_UNKNOWN,
   } mongoc_topology_description_type_t;

typedef struct _mongoc_topology_description_t
{
   mongoc_topology_description_type_t type;
   mongoc_server_description_t       *servers;
   char                              *set_name;
   bool                               compatible;
   char                              *compatibility_error;
} mongoc_topology_description_t;

typedef enum
   {
      MONGOC_SS_READ,
      MONGOC_SS_WRITE
   } mongoc_ss_optype_t;

void _mongoc_topology_description_init                  (mongoc_topology_description_t     *description);
void _mongoc_topology_description_destroy               (mongoc_topology_description_t     *description);
void _mongoc_topology_description_handle_ismaster       (mongoc_topology_description_t     *topology,
                                                         const bson_t                      *ismaster);
mongoc_server_description_t *_mongoc_topology_description_select (mongoc_topology_description_t *topology_description,o
                                                                  mongoc_ss_optype_t optype,
                                                                  const mongoc_read_prefs_t *read_pref,
                                                                  bson_error_t *error);
#endif
