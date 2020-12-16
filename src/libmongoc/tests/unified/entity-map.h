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

#include "bson/bson.h"
#include "mongoc-array-private.h"

typedef struct _event_t {
   char *type;
   char *command_name;
   bson_t *command;
   bson_t *reply;
   struct _event_t *next;
} event_t;

typedef struct _entity_t {
   char *id;
   char *type;
   void *value;
   bson_t *ignore_command_monitoring_events;
   struct _entity_t *next;
   event_t *events;
} entity_t;

/* Operations on the entity map enforce:
 * 1. Uniqueness. Attempting to create two entries with the same id is an error.
 * 2. Referential integrity. Attempting to get with an unknown id is an error.
 */
typedef struct {
   entity_t *entities;
} entity_map_t;

entity_map_t *
entity_map_new ();

void
entity_map_destroy (entity_map_t *em);

/* Creates an entry in the entity map based on what is specified in @bson.
 */
bool
entity_map_create (entity_map_t *em, bson_t *bson, bson_error_t *error);

/* Returns NULL and sets @error if @id does not map to an entry. */
entity_t *
entity_map_get (entity_map_t *em, const char *id, bson_error_t *error);
