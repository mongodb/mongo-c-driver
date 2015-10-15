/*
 * Copyright 2013 MongoDB, Inc.
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

#ifndef MONGOC_READ_PREFS_PRIVATE_H
#define MONGOC_READ_PREFS_PRIVATE_H

#if !defined (MONGOC_I_AM_A_DRIVER) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-cluster-private.h"
#include "mongoc-read-prefs.h"


BSON_BEGIN_DECLS

/* forward decl */
typedef struct _mongoc_topology_t mongoc_topology_t;

struct _mongoc_read_prefs_t
{
   mongoc_read_mode_t mode;
   bson_t             tags;
};

bool mongoc_read_prefs_primary0 (const mongoc_read_prefs_t *read_prefs);

bool apply_read_preferences (const mongoc_read_prefs_t *read_prefs,
                             bool is_write_command,
                             mongoc_topology_t *topology,
                             uint32_t server_id,
                             bson_t *query_bson,
                             mongoc_rpc_query_t *query_rpc,
                             bson_error_t *error);

BSON_END_DECLS


#endif /* MONGOC_READ_PREFS_PRIVATE_H */
