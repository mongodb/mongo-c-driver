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

#ifndef UNIFIED_ENTITY_MAP_H
#define UNIFIED_ENTITY_MAP_H

#include "bson/bson.h"
#include "mongoc/mongoc.h"
#include "mongoc-array-private.h"
#include "bsonutil/bson-match.h"
#include "test-diagnostics.h"

typedef struct _event_t {
   char *type;
   char *command_name;
   char *database_name;
   bson_t *command;
   bson_t *reply;
   struct _event_t *next;
} event_t;

typedef struct _entity_t {
   char *id;
   char *type;
   void *value;
   bson_t *ignore_command_monitoring_events;
   bool *observe_sensitive_commands;
   struct _entity_t *next;
   event_t *events;
   bson_t *lsid;
   char *session_client_id;
} entity_t;

/* Operations on the entity map enforce:
 * 1. Uniqueness. Attempting to create two entries with the same id is an error.
 * 2. Referential integrity. Attempting to get with an unknown id is an error.
 */
typedef struct {
   entity_t *entities;
   bool reduced_heartbeat;
} entity_map_t;

entity_map_t *
entity_map_new ();

void
entity_map_destroy (entity_map_t *em);

/* Creates an entry in the entity map based on what is specified in @bson.
 */
bool
entity_map_create (entity_map_t *em, bson_t *bson, bson_error_t *error);

/* Steals ownership of changestream. */
bool
entity_map_add_changestream (entity_map_t *em,
                             const char *id,
                             mongoc_change_stream_t *changestream,
                             bson_error_t *error);

/* Steals ownership of cursor. */
bool
entity_map_add_cursor (entity_map_t *em,
                       const char *id,
                       mongoc_cursor_t *cursor,
                       bson_error_t *error);

/* Copies val */
bool
entity_map_add_bson (entity_map_t *em,
                     const char *id,
                     bson_val_t *val,
                     bson_error_t *error);

/* Returns NULL and sets @error if @id does not map to an entry. */
entity_t *
entity_map_get (entity_map_t *em, const char *id, bson_error_t *error);

/* Removes an entity from the entity map. Returns false and sets @error if @id
 * does not map to an entry. */
bool
entity_map_delete (entity_map_t *em, const char *id, bson_error_t *error);

mongoc_client_t *
entity_map_get_client (entity_map_t *entity_map,
                       const char *id,
                       bson_error_t *error);

mongoc_database_t *
entity_map_get_database (entity_map_t *entity_map,
                         const char *id,
                         bson_error_t *error);

mongoc_collection_t *
entity_map_get_collection (entity_map_t *entity_map,
                           const char *id,
                           bson_error_t *error);

mongoc_change_stream_t *
entity_map_get_changestream (entity_map_t *entity_map,
                             const char *id,
                             bson_error_t *error);

mongoc_cursor_t *
entity_map_get_cursor (entity_map_t *entity_map,
                       const char *id,
                       bson_error_t *error);

mongoc_client_session_t *
entity_map_get_session (entity_map_t *entity_map,
                        const char *id,
                        bson_error_t *error);

bson_val_t *
entity_map_get_bson (entity_map_t *entity_map,
                     const char *id,
                     bson_error_t *error);

mongoc_gridfs_bucket_t *
entity_map_get_bucket (entity_map_t *entity_map,
                       const char *id,
                       bson_error_t *error);

bool
entity_map_match (entity_map_t *em,
                  const bson_val_t *expected,
                  const bson_val_t *actual,
                  bool allow_extra,
                  bson_error_t *error);

char *
event_list_to_string (event_t *events);

bool
entity_map_end_session (entity_map_t *em,
                        char *session_id,
                        bson_error_t *error);

char *
entity_map_get_session_client_id (entity_map_t *em,
                                  char *session_id,
                                  bson_error_t *error);

void
entity_map_set_reduced_heartbeat (entity_map_t *em, bool val);

void
entity_map_disable_event_listeners (entity_map_t *em);
#endif /* UNIFIED_ENTITY_MAP_H */
